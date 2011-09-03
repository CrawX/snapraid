/*
 * Copyright (C) 2011 Andrea Mazzoleni
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "portable.h"

#include "util.h"
#include "elem.h"
#include "state.h"

struct snapraid_scan {
	/**
	 * Counters of changes.
	 */
	unsigned count_equal;
	unsigned count_moved;
	unsigned count_change;
	unsigned count_remove;
	unsigned count_insert;

	tommy_list file_insert_list; /**< Files to insert. */
	tommy_list link_insert_list; /**< Links to insert. */
};

/**
 * Removes the specified file from the data set.
 */
static void scan_file_remove(struct snapraid_state* state, struct snapraid_disk* disk, struct snapraid_file* file)
{
	block_off_t i, j;
	unsigned diskmax = tommy_array_size(&state->diskarr);

	/* state changed */
	state->need_write = 1;

	/* free all the blocks of the file */
	for(i=0;i<file->blockmax;++i) {
		block_off_t block_pos = file->blockvec[i].parity_pos;

		/* adjust the first free position */
		if (disk->first_free_block > block_pos)
			disk->first_free_block = block_pos;

		/* clear the block */
		tommy_array_set(&disk->blockarr, block_pos, 0);

		/* invalidate the block of all the other disks */
		for(j=0;j<diskmax;++j) {
			struct snapraid_disk* oth_disk = tommy_array_get(&state->diskarr, j);
			struct snapraid_block* oth_block = disk_block_get(oth_disk, block_pos);
			if (oth_block) {
				/* remove the parity info for this block */
				block_flag_clear(oth_block, BLOCK_HAS_PARITY);
			}
		}
	}

	/* remove the file from the file containers */
	tommy_hashdyn_remove_existing(&disk->inodeset, &file->nodeset);
	tommy_list_remove_existing(&disk->filelist, &file->nodelist);

	/* deallocate */
	file_free(file);
}

/**
 * Inserts the specified file in the data set.
 */
static void scan_file_insert(struct snapraid_state* state, struct snapraid_disk* disk, struct snapraid_file* file)
{
	block_off_t i;
	block_off_t block_max;
	block_off_t block_pos;

	/* state changed */
	state->need_write = 1;

	/* allocate the blocks of the file */
	block_pos = disk->first_free_block;
	block_max = tommy_array_size(&disk->blockarr);
	for(i=0;i<file->blockmax;++i) {
		/* find a free block */
		while (block_pos < block_max && tommy_array_get(&disk->blockarr, block_pos) != 0)
			++block_pos;

		/* if not found, allocate a new one */
		if (block_pos == block_max) {
			++block_max;
			tommy_array_grow(&disk->blockarr, block_max);
		}

		/* set it */
		file->blockvec[i].parity_pos = block_pos;
		tommy_array_set(&disk->blockarr, block_pos, &file->blockvec[i]);
	}
	if (file->blockmax) {
		/* set the new free position, but only if allocated something */
		disk->first_free_block = block_pos + 1;
	}

	/* insert the file in the file containers */
	tommy_hashdyn_insert(&disk->inodeset, &file->nodeset, file, file_inode_hash(file->inode));
	tommy_list_insert_tail(&disk->filelist, &file->nodelist, file);
}

/**
 * Processes a file.
 */
static void scan_file(struct snapraid_scan* scan, struct snapraid_state* state, int output, struct snapraid_disk* disk, const char* sub, const struct stat* st)
{
	struct snapraid_file* file;
	uint64_t inode;

	/* check if the file already exists */
	inode = st->st_ino;
	file = tommy_hashdyn_search(&disk->inodeset, file_inode_compare, &inode, file_inode_hash(inode));
	if (file) {
		/* check if multiple files have the same inode */
		if (file_flag_has(file, FILE_IS_PRESENT)) {
			if (st->st_nlink > 1) {
				printf("warning: Ignored hardlink '%s%s'\n", disk->dir, sub);
				return;
			} else {
				fprintf(stderr, "Internal inode '%"PRIu64"' inconsistency for file '%s%s'\n", inode, disk->dir, sub);
				exit(EXIT_FAILURE);
			}
		}

		/* check if the file is not changed */
		if (file->size == st->st_size && file->mtime == st->st_mtime) {
			/* mark as present */
			file_flag_set(file, FILE_IS_PRESENT);

			/* if the path is different, it means a moved file */
			if (strcmp(file->sub, sub) != 0) {
				++scan->count_moved;

				if (state->gui) {
					fprintf(stderr, "scan:move:%s:%s:%s\n", disk->name, file->sub, sub);
					fflush(stderr);
				}
				if (output) {
					printf("Move '%s%s' '%s%s'\n", disk->dir, file->sub, disk->dir, sub);
				}

				/* save the new name */
				pathcpy(file->sub, sizeof(file->sub), sub);

				/* we have to save the new name */
				state->need_write = 1;
			} else {
				/* otherwise it's equal */
				++scan->count_equal;

				if (state->gui) {
					fprintf(stderr, "scan:equal:%s:%s\n", disk->name, file->sub);
					fflush(stderr);
				}
			}

			/* nothing more to do */
			return;
		} else {
			/* do a safety check to ensure that the common ext4 case of zeroing */
			/* the size of a file after a crash doesn't propagate to the backup */
			if (file->size != 0 && st->st_size == 0) {
				/* do the check ONLY if the name is the same */
				/* otherwise it could be a deleted and recreated file */
				if (strcmp(file->sub, sub) == 0) {
					if (!state->force_zero) {
						fprintf(stderr, "The file '%s%s' has now zero size!\n", disk->dir, sub);
						fprintf(stderr, "If you really want to sync, use 'snapraid --force-zero sync'\n");
						exit(EXIT_FAILURE);
					}
				}
			}

			if (strcmp(file->sub, sub) == 0) {
				/* if the name is the same, it's an update */
				if (state->gui) {
					fprintf(stderr, "scan:update:%s:%s\n", disk->name, file->sub);
					fflush(stderr);
				}
				if (output) {
					printf("Update '%s%s'\n", disk->dir, file->sub);
				}

				++scan->count_change;
			} else {
				/* if the name is different, it's an inode reuse */
				if (state->gui) {
					fprintf(stderr, "scan:remove:%s:%s\n", disk->name, file->sub);
					fprintf(stderr, "scan:add:%s:%s\n", disk->name, sub);
					fflush(stderr);
				}
				if (output) {
					printf("Remove '%s%s'\n", disk->dir, file->sub);
					printf("Add '%s%s'\n", disk->dir, sub);
				}

				++scan->count_remove;
				++scan->count_insert;
			}

			/* remove it */
			scan_file_remove(state, disk, file);

			/* and continue to reinsert it */
		}
	}

	/* insert it */
	file = file_alloc(state->block_size, sub, st->st_size, st->st_mtime, st->st_ino);

	/* mark it as present */
	file_flag_set(file, FILE_IS_PRESENT);

	/* insert it in the delayed insert list */
	tommy_list_insert_tail(&scan->file_insert_list, &file->nodelist, file);
}

/**
 * Removes the specified link from the data set.
 */
static void scan_link_remove(struct snapraid_state* state, struct snapraid_disk* disk, struct snapraid_link* link)
{
	/* state changed */
	state->need_write = 1;

	/* remove the file from the link containers */
	tommy_hashdyn_remove_existing(&disk->linkset, &link->nodeset);
	tommy_list_remove_existing(&disk->linklist, &link->nodelist);

	/* deallocate */
	link_free(link);
}

/**
 * Inserts the specified link in the data set.
 */
static void scan_link_insert(struct snapraid_state* state, struct snapraid_disk* disk, struct snapraid_link* link)
{
	/* state changed */
	state->need_write = 1;

	/* insert the link in the link containers */
	tommy_hashdyn_insert(&disk->linkset, &link->nodeset, link, link_name_hash(link->sub));
	tommy_list_insert_tail(&disk->linklist, &link->nodelist, link);
}

/**
 * Processes a symbolic link.
 */
static void scan_link(struct snapraid_scan* scan, struct snapraid_state* state, int output, struct snapraid_disk* disk, const char* sub, const char* linkto)
{
	struct snapraid_link* link;

	/* check if the link already exists */
	link = tommy_hashdyn_search(&disk->linkset, link_name_compare, sub, link_name_hash(sub));
	if (link) {
		/* check if multiple files have the same inode */
		if (link_flag_has(link, FILE_IS_PRESENT)) {
			fprintf(stderr, "Internal inconsistency for symlink '%s%s'\n", disk->dir, sub);
			exit(EXIT_FAILURE);
		}

		/* mark as present */
		link_flag_set(link, FILE_IS_PRESENT);

		/* check if the link is not changed */
		if (strcmp(link->linkto, linkto) == 0) {
			/* it's equal */
			++scan->count_equal;

			if (state->gui) {
				fprintf(stderr, "scan:equal:%s:%s\n", disk->name, link->sub);
				fflush(stderr);
			}

			/* nothing more to do */
			return;
		} else {
			/* it's an update */
			if (state->gui) {
				fprintf(stderr, "scan:update:%s:%s\n", disk->name, link->sub);
				fflush(stderr);
			}
			if (output) {
				printf("Update '%s%s'\n", disk->dir, link->sub);
			}

			++scan->count_change;

			/* update it */
			pathcpy(link->linkto, sizeof(link->linkto), linkto);

			/* nothing more to do */
			return;
		}
	}

	/* insert it */
	link = link_alloc(sub, linkto);

	/* mark it as present */
	link_flag_set(link, FILE_IS_PRESENT);

	/* insert it in the delayed insert list */
	tommy_list_insert_tail(&scan->link_insert_list, &link->nodelist, link);
}

/**
 * Processes a directory.
 */
static void scan_dir(struct snapraid_scan* scan, struct snapraid_state* state, int output, struct snapraid_disk* disk, const char* dir, const char* sub)
{
	DIR* d;

	d = opendir(dir);
	if (!d) {
		fprintf(stderr, "Error opening directory '%s'. %s.\n", dir, strerror(errno));
		fprintf(stderr, "You can exclude it in the config file with:\n\texclude /%s/\n", sub);
		exit(EXIT_FAILURE);
	}
   
	while (1) { 
		char path_next[PATH_MAX];
		char sub_next[PATH_MAX];
		struct stat st;
		const char* name;
		struct dirent* dd;

		/* clear errno to detect errneous conditions */
		errno = 0;
		dd = readdir(d);
		if (dd == 0 && errno != 0) {
			fprintf(stderr, "Error reading directory '%s'. %s.\n", dir, strerror(errno));
			fprintf(stderr, "You can exclude it in the config file with:\n\texclude /%s/\n", sub);
			exit(EXIT_FAILURE);
		}
		if (dd == 0 && errno == 0) {
			break; /* finished */
		}

		/* skip "." and ".." files */
		name = dd->d_name;
		if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0)))
			continue;

		pathprint(path_next, sizeof(path_next), "%s%s", dir, name);
		pathprint(sub_next, sizeof(sub_next), "%s%s", sub, name);

		/* check for not supported file names, limitation derived from the content file format */
		if (name[0] == 0 || strchr(name, '\n') != 0 || name[strlen(name)-1] == '\r') {
			fprintf(stderr, "Unsupported name '%s' in file '%s'.\n", name, path_next);
			exit(EXIT_FAILURE);
		}

		/* get info about the file */
		if (lstat(path_next, &st) != 0) {
			fprintf(stderr, "Error in stat file/directory '%s'. %s.\n", path_next, strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (S_ISREG(st.st_mode)) {
			if (filter_path(&state->filterlist, sub_next, 0) == 0) {

				/* check for read permission */
				if (access(path_next, R_OK) != 0) {
					fprintf(stderr, "warning: Ignoring, for missing read permission, file '%s'\n", path_next);
					continue;
				}

#if HAVE_LSTAT_INODE
				/* get inode info about the file, Windows needs an additional step */
				if (lstat_inode(path_next, &st) != 0) {
					fprintf(stderr, "Error in stat_inode file '%s'. %s.\n", path_next, strerror(errno));
					exit(EXIT_FAILURE);
				}
#endif

				scan_file(scan, state, output, disk, sub_next, &st);
			} else {
				if (state->verbose) {
					printf("Excluding file '%s'\n", path_next);
				}
			}
		} else if (S_ISLNK(st.st_mode)) {
			if (filter_path(&state->filterlist, sub_next, 0) == 0) {
				char subnew[PATH_MAX];
				int ret;

				ret = readlink(path_next, subnew, sizeof(subnew));
				if (ret >= PATH_MAX) {
					fprintf(stderr, "Error in readlink file '%s'. Symlink too long.\n", path_next);
					exit(EXIT_FAILURE);
				}
				if (ret < 0) {
					fprintf(stderr, "Error in readlink file '%s'. %s.\n", path_next, strerror(errno));
					exit(EXIT_FAILURE);
				}

				/* readlink doesn't put the final 0 */
				subnew[ret] = 0;

				scan_link(scan, state, output, disk, sub_next, subnew);
			} else {
				if (state->verbose) {
					printf("Excluding file '%s'\n", path_next);
				}
			}
		} else if (S_ISDIR(st.st_mode)) {
			if (filter_path(&state->filterlist, sub_next, 1) == 0) {
				pathslash(path_next, sizeof(path_next));
				pathslash(sub_next, sizeof(sub_next));
				scan_dir(scan, state, output, disk, path_next, sub_next);
			} else {
				if (state->verbose) {
					printf("Excluding directory '%s'\n", path_next);
				}
			}
		} else {
			if (filter_path(&state->filterlist, sub_next, 0) == 0) {
				fprintf(stderr, "warning: Ignoring special file '%s'\n", path_next);
			} else {
				if (state->verbose) {
					printf("Excluding special file '%s'\n", path_next);
				}
			}
		}
	}

	if (closedir(d) != 0) {
		fprintf(stderr, "Error closing directory '%s'. %s.\n", dir, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void state_scan(struct snapraid_state* state, int output)
{
	unsigned diskmax = tommy_array_size(&state->diskarr);
	unsigned i;
	struct snapraid_scan* scan;

	scan = malloc_nofail(diskmax * sizeof(struct snapraid_scan));
	for(i=0;i<diskmax;++i) {
		scan[i].count_equal = 0;
		scan[i].count_moved = 0;
		scan[i].count_change = 0;
		scan[i].count_remove = 0;
		scan[i].count_insert = 0;
		tommy_list_init(&scan[i].file_insert_list);
		tommy_list_init(&scan[i].link_insert_list);
	}

	for(i=0;i<diskmax;++i) {
		struct snapraid_disk* disk = tommy_array_get(&state->diskarr, i);
		tommy_node* node;

		printf("Scanning disk %s...\n", disk->name);

		scan_dir(&scan[i], state, output, disk, disk->dir, "");

		/* check for removed files */
		node = disk->filelist;
		while (node) {
			struct snapraid_file* file = node->data;

			/* next node */
			node = node->next;

			/* remove if not present */
			if (!file_flag_has(file, FILE_IS_PRESENT)) {
				++scan[i].count_remove;

				if (state->gui) {
					fprintf(stderr, "scan:remove:%s:%s\n", disk->name, file->sub);
					fflush(stderr);
				}
				if (output) {
					printf("Remove '%s%s'\n", disk->dir, file->sub);
				}

				scan_file_remove(state, disk, file);
			}
		}

		/* check for removed links */
		node = disk->linklist;
		while (node) {
			struct snapraid_link* link = node->data;

			/* next node */
			node = node->next;

			/* remove if not present */
			if (!link_flag_has(link, FILE_IS_PRESENT)) {
				++scan[i].count_remove;

				if (state->gui) {
					fprintf(stderr, "scan:remove:%s:%s\n", disk->name, link->sub);
					fflush(stderr);
				}
				if (output) {
					printf("Remove '%s%s'\n", disk->dir, link->sub);
				}

				scan_link_remove(state, disk, link);
			}
		}

		/* insert all the new files, we insert them only after the deletion */
		node = scan[i].file_insert_list;
		while (node) {
			struct snapraid_file* file = node->data;

			/* next node */
			node = node->next;

			/* create the new file */
			++scan->count_insert;

			if (state->gui) {
				fprintf(stderr, "scan:add:%s:%s\n", disk->name, file->sub);
				fflush(stderr);
			}
			if (output) {
				printf("Add '%s%s'\n", disk->dir, file->sub);
			}

			/* insert it */
			scan_file_insert(state, disk, file);
		}

		/* insert all the new links */
		node = scan[i].link_insert_list;
		while (node) {
			struct snapraid_link* link = node->data;

			/* next node */
			node = node->next;

			/* create the new link */
			++scan->count_insert;

			if (state->gui) {
				fprintf(stderr, "scan:add:%s:%s\n", disk->name, link->sub);
				fflush(stderr);
			}
			if (output) {
				printf("Add '%s%s'\n", disk->dir, link->sub);
			}

			/* insert it */
			scan_link_insert(state, disk, link);
		}
	}

	/* checks for disks where all the previously existing files where removed */
	if (!state->force_empty) {
		int has_empty = 0;
		for(i=0;i<diskmax;++i) {
			struct snapraid_disk* disk = tommy_array_get(&state->diskarr, i);

			if (scan[i].count_equal == 0 && scan[i].count_moved == 0 && scan[i].count_remove != 0) {
				if (!has_empty) {
					has_empty = 1;
					fprintf(stderr, "All the files previously present in disk '%s' at dir '%s'", disk->name, disk->dir);
				} else {
					fprintf(stderr, ", disk '%s' at dir '%s'", disk->name, disk->dir);
				}
			}
		}
		if (has_empty) {
			fprintf(stderr, " are now missing or rewritten!\n");
			fprintf(stderr, "This happens with an empty disk or when all the files are recreated after a 'fix' command.\n");
			fprintf(stderr, "If you really want to sync, use 'snapraid --force-empty sync'.\n");
			exit(EXIT_FAILURE);
		}
	}

	if (state->verbose || output) {
		struct snapraid_scan total;

		total.count_equal = 0;
		total.count_moved = 0;
		total.count_change = 0;
		total.count_remove = 0;
		total.count_insert = 0;

		for(i=0;i<diskmax;++i) {
			total.count_equal += scan[i].count_equal;
			total.count_moved += scan[i].count_moved;
			total.count_change += scan[i].count_change;
			total.count_remove += scan[i].count_remove;
			total.count_insert += scan[i].count_insert;
		}

		if (state->verbose) {
			printf("\tequal %d\n", total.count_equal);
			printf("\tmoved %d\n", total.count_moved);
			printf("\tchanged %d\n", total.count_change);
			printf("\tremoved %d\n", total.count_remove);
			printf("\tadded %d\n", total.count_insert);
		}

		if (output) {
			if (!total.count_moved && !total.count_change && !total.count_remove && !total.count_insert) {
				printf("No difference\n");
			}
		}
	}

	free(scan);
}


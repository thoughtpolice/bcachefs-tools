
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <nih/command.h>
#include <nih/option.h>

#include <uuid/uuid.h>

#include "bcache.h"
#include "bcacheadm-query.h"

static char *cset_dir = "/sys/fs/bcache";
static bool list_devs = false;
static const char *internal_uuid = NULL;

NihOption opts_list[] = {
	{'d', "dir", N_("directory"), NULL, NULL, &cset_dir, NULL},
	{0, "list-devs", N_("list all devices in the cache sets as well"), NULL, NULL, &list_devs, NULL},
	{0, "internal_uuid", N_("Show the internal UUID for the given cacheset UUID"), NULL, "UUID", &internal_uuid, NULL},
	NIH_OPTION_LAST
};

static void list_cacheset_devs(char *cset_dir, char *cset_name, bool parse_dev_name)
{
	DIR *cachedir, *dir;
	struct stat cache_stat;
	char entry[MAX_PATH];
	struct dirent *ent;
	snprintf(entry, MAX_PATH, "%s/%s", cset_dir, cset_name);

	if((dir = opendir(entry)) != NULL) {
		while((ent = readdir(dir)) != NULL) {
			char buf[MAX_PATH];
			int len;
			char *tmp;

			/*
			 * We are looking for all cache# directories
			 * do a strlen < 9 to skip over other entries
			 * that also start with "cache"
			 */
			if(strncmp(ent->d_name, "cache", 5) ||
					!(strlen(ent->d_name) < 9))
				continue;

			snprintf(entry, MAX_PATH, "%s/%s/%s",
					cset_dir,
					cset_name,
					ent->d_name);

			if((cachedir = opendir(entry)) == NULL)
				continue;

			if(stat(entry, &cache_stat))
				continue;

			if((len = readlink(entry, buf, sizeof(buf) - 1)) !=
					-1) {
				buf[len] = '\0';
				if(parse_dev_name) {
					tmp = dev_name(buf);
					printf("/dev%s\n", tmp);
					free(tmp);
				} else {
					printf("\t%s\n", buf);
				}
			}
		}
	}
}

static char *list_cachesets(char *cset_dir, bool list_devs)
{
	struct dirent *ent;
	DIR *dir;
	char *err = NULL;

	dir = opendir(cset_dir);
	if (!dir) {
		err = "Failed to open cacheset dir";
		goto err;
	}

	while ((ent = readdir(dir)) != NULL) {
		struct stat statbuf;
		char entry[MAX_PATH];

		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;

		snprintf(entry, MAX_PATH, "%s/%s", cset_dir, ent->d_name);
		if(stat(entry, &statbuf) == -1) {
			err = "Failed to stat cacheset subdir";
			goto err;
		}

		if (S_ISDIR(statbuf.st_mode)) {
			printf("%s\n", ent->d_name);

			if(list_devs) {
				list_cacheset_devs(cset_dir, ent->d_name, true);
			}
		}
	}

err:
	closedir(dir);
	return err;
}

static char *read_stat_dir(DIR *dir, char *stats_dir, char *stat_name, char *ret)
{
	struct stat statbuf;
	char entry[MAX_PATH];
	char *err = NULL;

	snprintf(entry, MAX_PATH, "%s/%s", stats_dir, stat_name);
	if(stat(entry, &statbuf) == -1) {
		char tmp[MAX_PATH];
		snprintf(tmp, MAX_PATH, "Failed to stat %s\n", entry);
		err = strdup(tmp);
		goto err;
	}

	if (S_ISREG(statbuf.st_mode)) {
		FILE *fp = NULL;

		fp = fopen(entry, "r");
		if(!fp) {
			/* If we can't open the file, this is probably because
			 * of permissions, just move to the next file */
			return NULL;
		}

		while(fgets(ret, MAX_PATH, fp));
		fclose(fp);
	}
err:
	return err;
}

int cmd_list(NihCommand *command, char *const *args)
{
	char *err = NULL;

	if (internal_uuid) {
		char uuid_path[MAX_PATH];
		DIR *uuid_dir;
		char buf[MAX_PATH];

		snprintf(uuid_path, MAX_PATH, "%s/%s", cset_dir, internal_uuid);

		err = "uuid does not exist";
		if((uuid_dir = opendir(uuid_path)) == NULL)
			goto err;

		err = read_stat_dir(uuid_dir, uuid_path, "/internal/internal_uuid", buf);
		if (err)
			goto err;
		printf("%s", buf);
		return 0;
	}

	err = list_cachesets(cset_dir, list_devs);
	if (err)
		goto err;

	return 0;

err:
	printf("bcache_list_cachesets error :%s\n", err);
	return -1;
}

static bool force_csum = false;
static bool uuid_only = false;
static bool query_brief = false;

NihOption opts_query[] = {
	{'f', "force_csum", N_("force_csum"), NULL, NULL, &force_csum, NULL},
	{'u', "uuid-only", N_("only print out the uuid for the devices, not the whole superblock"), NULL, NULL, &uuid_only, NULL},
	{'b', "brief", N_("only print out the cluster,server,and disk uuids"), NULL, NULL, &query_brief, NULL},
	NIH_OPTION_LAST
};

int cmd_query(NihCommand *command, char *const *args)
{
	int i;

	if (query_brief)
		printf("%-10s%-40s%-40s%-40s\n", "dev name", "disk uuid",
				"server uuid", "cluster uuid");

	for (i = 0; args[i] != NULL; i++) {
		char dev_uuid[40];
		struct cache_sb *sb = query_dev(args[i], force_csum,
				!query_brief, uuid_only, dev_uuid);

		if (!sb) {
			printf("error opening the superblock for %s\n",
					args[i]);
			return -1;
		}

		if (uuid_only) {
			printf("%s\n", dev_uuid);
		} else if (query_brief) {
			char set_uuid_str[40], dev_uuid_str[40];
			char *clus_uuid = (char *)sb->label;

			uuid_unparse(sb->user_uuid.b, set_uuid_str);
			uuid_unparse(sb->disk_uuid.b, dev_uuid_str);
			if (!strcmp(clus_uuid, ""))
				clus_uuid = "None";

			printf("%-10s%-40s%-40s%-40s\n", args[i],
					dev_uuid_str,
					set_uuid_str,
					clus_uuid);
		}
		free(sb);
	}

	return 0;
}

static bool status_all = false;

NihOption opts_status[] = {
	{'a', "all", N_("all"), NULL, NULL, &status_all, NULL},
	NIH_OPTION_LAST
};

int cmd_status(NihCommand *command, char *const *args)
{
	int i, dev_count = 0, seq, cache_count = 0;
	struct cache_sb *seq_sb = NULL;
	char cache_path[MAX_PATH];
	char *dev_names[MAX_DEVS];
	char *dev_uuids[MAX_DEVS];
	char intbuf[4];
	char set_uuid[40];

	for (i = 0; args[i] != NULL; i++) {
		struct cache_sb *sb = query_dev(args[i], false, false,
				false, NULL);

		if (!sb) {
			printf("Unable to open superblock, bad path\n");
			return -1;
		}

		if (!seq_sb || sb->seq > seq) {
			seq = sb->seq;
			seq_sb = sb;
		} else
			free(sb);
	}

	if (!seq_sb) {
		printf("Unable to find a superblock\n");
		return -1;
	} else {
		uuid_unparse(seq_sb->user_uuid.b, set_uuid);
		printf("%-50s%-15s%-4s\n", "uuid", "state", "tier");
	}

	snprintf(intbuf, 4, "%d", i);
	snprintf(cache_path, MAX_PATH, "%s/%s/%s", cset_dir, set_uuid,
			"cache0");

	/*
	 * Get a list of all the devices from sysfs first, then
	 * compare it to the list we get back from the most up
	 * to date superblock. If there are any devices in the superblock
	 * that are not in sysfs, print out 'missing'
	 */
	while (true) {
		char buf[MAX_PATH];
		int len;
		DIR *cache_dir;

		if(((cache_dir = opendir(cache_path)) == NULL) &&
		    cache_count > MAX_DEVS)
			break;

		if (cache_dir)
			closedir(cache_dir);

		if((len = readlink(cache_path, buf, sizeof(buf) - 1)) != -1) {
			struct cache_sb *sb;
			char dev_uuid[40];
			char dev_path[32];

			buf[len] = '\0';
			dev_names[dev_count] = dev_name(buf);
			snprintf(dev_path, MAX_PATH, "%s/%s", "/dev",
					dev_names[dev_count]);
			sb = query_dev(dev_path, false, false,
					true, dev_uuid);
			if (!sb) {
				printf("error reading %s\n", dev_path);
				return -1;
			} else
				free(sb);

			dev_uuids[dev_count] = strdup(dev_uuid);
		        dev_count++;
		}

		cache_path[strlen(cache_path) - strlen(intbuf)] = 0;
		cache_count++;

		snprintf(intbuf, 4, "%d", cache_count);
		strcat(cache_path, intbuf);
	}

	for (i = 0; i < seq_sb->nr_in_set; i++) {
		char uuid_str[40];
		struct cache_member *m = seq_sb->members + i;
		char dev_state[32];
		int j;

		uuid_unparse(m->uuid.b, uuid_str);
		snprintf(dev_state, MAX_PATH, "%s",
                         cache_state[CACHE_STATE(m)]);

		for (j = 0; j < dev_count; j++) {
			if (!strcmp(uuid_str, dev_uuids[j])) {
				break;
			} else if (j == dev_count - 1) {
				if (!strcmp(cache_state[CACHE_STATE(m)], "active"))
					snprintf(dev_state, MAX_PATH, "%s", "missing");
				break;
			}
		}

		printf("%-50s%-15s%-4llu\n", uuid_str, dev_state,
				CACHE_TIER(m));
	}

	if (seq_sb)
		free(seq_sb);
	for (i = 0; i < dev_count; i++) {
		free(dev_names[i]);
		free(dev_uuids[i]);
	}

	return 0;
}

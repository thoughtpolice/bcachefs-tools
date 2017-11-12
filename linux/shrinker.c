
#include <stdio.h>

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/shrinker.h>

#include "tools-util.h"

static LIST_HEAD(shrinker_list);
static DEFINE_MUTEX(shrinker_lock);

int register_shrinker(struct shrinker *shrinker)
{
	mutex_lock(&shrinker_lock);
	list_add_tail(&shrinker->list, &shrinker_list);
	mutex_unlock(&shrinker_lock);
	return 0;
}

void unregister_shrinker(struct shrinker *shrinker)
{
	mutex_lock(&shrinker_lock);
	list_del(&shrinker->list);
	mutex_unlock(&shrinker_lock);
}

struct meminfo {
	u64		total;
	u64		available;

};

static u64 parse_meminfo_line(const char *line)
{
	u64 v;

	if (sscanf(line, " %llu kB", &v) < 1)
		die("sscanf error");
	return v << 10;
}

static struct meminfo read_meminfo(void)
{
	struct meminfo ret = { 0 };
	size_t len, n = 0;
	char *line = NULL;
	const char *v;
	FILE *f;

	f = fopen("/proc/meminfo", "r");
	if (!f)
		die("error opening /proc/meminfo: %m");

	while ((len = getline(&line, &n, f)) != -1) {
		if ((v = strcmp_prefix(line, "MemTotal:")))
			ret.total = parse_meminfo_line(v);

		if ((v = strcmp_prefix(line, "MemAvailable:")))
			ret.available = parse_meminfo_line(v);
	}

	fclose(f);
	free(line);

	return ret;
}

void run_shrinkers(void)
{
	struct shrinker *shrinker;
	struct meminfo info = read_meminfo();
	s64 want_shrink = (info.total >> 2) - info.available;

	if (want_shrink <= 0)
		return;

	mutex_lock(&shrinker_lock);
	list_for_each_entry(shrinker, &shrinker_list, list) {
		struct shrink_control sc = {
			.nr_to_scan = want_shrink >> PAGE_SHIFT
		};

		shrinker->scan_objects(shrinker, &sc);
	}
	mutex_unlock(&shrinker_lock);
}

#ifndef DIRENT_H
#define DIRENT_H

typedef struct DIR DIR;

#define DT_UNKNOWN 0
#define DT_DIR     1
#define DT_REG     2
#define DT_LNK     3

struct dirent {
	unsigned char d_type;
	char d_name[PATH_MAX];	// file name
};

DIR *opendir(const char *dirname);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

#endif /* DIRENT_H */

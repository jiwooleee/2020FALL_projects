#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
// strcmp()
#include <string.h>
// getcwd()
#include <unistd.h>
// PATH_MAX
#include <linux/limits.h>
// struct passwd
#include <pwd.h>
// struct group
#include <grp.h>

int main(int argc, char** argv) {
	// ls
	if (argc == 1) {
		DIR *dir;
		struct dirent *sd;

		// open current directory
		dir = opendir(".");

		if (!dir) {
			perror("open directory");
			exit(1);
		}

		int num_files = 0;
		while ((sd=readdir(dir))) {
			if (strcmp(sd->d_name, ".") != 0 && strcmp(sd->d_name, "..") != 0) {
				num_files++;
				// -10 left, plus for right
				printf("%-10s  ", sd->d_name);
				// three file names in a row
				if ((num_files % 3) == 0) {
					printf("\n");
				}

			}
		}
		printf("\n");
		closedir(dir);
		return 0;
	}
	// ls -l
	if (argc == 2 && strcmp(argv[1], "-l") == 0) {

		// DIR is hidden(?)
		DIR *dir;
		struct dirent *sd;
		char buf[PATH_MAX];

		// current directory == "."
		// how to get current directory name using getcwd
		// or just dir = opendir(".");
		if (!getcwd(buf, PATH_MAX)) {
			perror("get current directory");
			exit(1);
		}

		// open current directory
		dir = opendir(buf);

		if (!dir) {
			perror("open directory");
			exit(1);
		}

		char* filename, *owner, *group;
		struct stat st;
		struct passwd *pwd;
		struct group *grp;
		while ((sd=readdir(dir))) {
			// don't print out . / .. directories
			// . == current, .. == parent
			if (strcmp(sd->d_name, ".") != 0 && strcmp(sd->d_name, "..") != 0) {
				filename = sd->d_name;

				// check if stat failed
				if (stat(filename, &st) == -1) {
					perror("stat");
					exit(1);
				}

				// print file permissions, owner, group, size, name

				// file permissions == st_mode
				// is directory
				printf((S_ISDIR(st.st_mode)) ? "d" : "-");
				// user rwx
				printf((st.st_mode & S_IRUSR) ? "r" : "-");
				printf((st.st_mode & S_IWUSR) ? "w" : "-");
				printf((st.st_mode & S_IXUSR) ? "x" : "-");
				// user rwx
				printf((st.st_mode & S_IRGRP) ? "r" : "-");
				printf((st.st_mode & S_IWGRP) ? "w" : "-");
				printf((st.st_mode & S_IXGRP) ? "x" : "-");
				// other rwx
				printf((st.st_mode & S_IROTH) ? "r" : "-");
				printf((st.st_mode & S_IWOTH) ? "w" : "-");
				printf((st.st_mode & S_IXOTH) ? "x" : "-");

				// owner user ID == st_uid

				pwd = getpwuid(st.st_uid);
				owner = pwd->pw_name;

				// owner group ID == st_gid

				grp = getgrgid(st.st_gid);
				group = grp->gr_name;

				// size in bytes == st_size


				printf("  %-8s  %-8s  %-8ld  %-8s\n", owner, group, st.st_size, filename);
			}
		}
		closedir(dir);
		return 0;
	}
	else {
		printf("usage: ./ls OR ./ls -l for a more detailed list\n");
		exit(1);
	}
}

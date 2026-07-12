//
//	cpu over/underclock (based on code from eggs)
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <limits.h>
#include <errno.h>



#define GOVERNOR_CPUSPEED_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"
#define GOVERNOR_PATH          "/sys/devices/system/cpu/cpufreq/policy0/scaling_governor"


void putFile(char* path, char* contents) {
	FILE* file = fopen(path, "w");
	if (file) {
		fputs(contents, file);
		fclose(file);
	}
}

void putInt(char* path, int value) {
	char buffer[8];
	sprintf(buffer, "%d", value);
	putFile(path, buffer);
}

int cpu_opps[] = {480000, 720000, 936000, 1008000, 1104000, 1200000, 1320000, 1416000, 1512000};

int main(int argc, char* argv[]) {
	if (argc<2) {
		printf("Usage: %s <freq>\n", argv[0]);
		for (int i=0; i<9 ; i++) {
			printf("  %8i\n", cpu_opps[i]);
		}
		return 0;
	}
	
	int clk = 1008000; // default
	char *p;
	errno = 0;
	long arg = strtol(argv[1], &p, 10);
	
	if (errno != 0 || *p != '\0' || arg > INT_MAX || arg < INT_MIN); // buh
	else clk = arg;
	
	char cmd[512];
	//sudo sh -c "echo -n 1512000 > /sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"
	sprintf(cmd,"sudo sh -c \"echo -n userspace > %s \" ; sudo sh -c \"echo %i > %s\"", GOVERNOR_PATH, clk, GOVERNOR_CPUSPEED_PATH);
	system(cmd);
	putInt("/tmp/new_cpu_freq.txt", clk);
	return 0;
}

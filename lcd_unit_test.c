#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#define log(msg, ...) fprintf(stdout, __FILE__ ":%s():[%d]:" msg, __func__, __LINE__, __VA_ARGS__)

FILE *lcd;

void test(void)
{
	int k;

	// a lot of hello's
	log("hello world test\n", 1);
	rewind(lcd);
	fprintf(lcd, "\eJ\eH"); // escape seq to clear screen and goto home
	fprintf(lcd, "hello world\n");
	fprintf(lcd, "hello world\n");
	fflush(lcd);
	sleep(5);
	rewind(lcd);
	fprintf(lcd, "           \n");
	fprintf(lcd, "           \n");
	fflush(lcd);

	// random number test
	log("random number test\n", 1);
	srand(time(NULL));
	fseek(lcd, 0x11, SEEK_SET);
	fprintf(lcd, "random test:\n");
	for (k = 0; k < 10; k++)
	{
		fseek(lcd, 0x20, SEEK_SET);
		fprintf(lcd, "\t%1.4f\t%1.2f\n", 
			(float)rand()/(float)RAND_MAX,
			(float)rand()/(float)RAND_MAX);
		fflush(lcd);
		sleep(1);
	}

	// long test
	log("long test\n", 1);
	rewind(lcd);
	fprintf(lcd, "%1.48f", (float)rand()/(float)RAND_MAX);
	fflush(lcd);
	sleep(2);
	for (k = 0; k < 46; k++)
	{
		usleep(100e3);
		fprintf(lcd, "\b0");
		fprintf(lcd, "\b");
		fflush(lcd);
	}
}

int main(int argc, char **argv)
{
	lcd = fopen("/dev/lcd", "r+");
	if (lcd == NULL)
		exit(EXIT_FAILURE);

	test();

	fclose(lcd);
	return 0;
}


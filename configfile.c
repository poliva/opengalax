#include "opengalax.h"

#define CONFIG_FILE "/etc/opengalax.conf"
#define MAXLEN 1024

static const conf_data default_config = {
	/* serial_device */ "/dev/serio_raw0",
	/* uinput_device */ "/dev/uinput",
	/* rightclick_enable */	1,
	/* rightclick_duration */ 350,
	/* rightclick_range */ 10,
};

static const calibration_data default_calibration = {
        /* xmin */ 0,
        /* xmax */ 2047,
        /* ymin */ 0,
        /* ymax */ 2047,
};

int create_config_file (char* file) {
        FILE* fd;

        fd = fopen(file, "w");
        if (fd == NULL)
                return 0;

        fprintf(fd, "# opengalax configuration file\n");
        fprintf(fd, "\n#### config data:\n");
        fprintf(fd, "serial_device=%s\n", default_config.serial_device);
        fprintf(fd, "uinput_device=%s\n", default_config.uinput_device);
	fprintf(fd, "rightclick_enable=%d\n", default_config.rightclick_enable);
	fprintf(fd, "rightclick_duration=%d\n", default_config.rightclick_duration);
	fprintf(fd, "rightclick_range=%d\n", default_config.rightclick_range);
        fprintf(fd, "\n#### calibration data:\n");
	fprintf(fd, "# - values should range from 0 to 2047\n");
	fprintf(fd, "# - right/bottom must be bigger than left/top\n");
	fprintf(fd, "# left edge value:\n");
        fprintf(fd, "xmin=%d\n", default_calibration.xmin);
	fprintf(fd, "# right edge value:\n");
        fprintf(fd, "xmax=%d\n", default_calibration.xmax);
	fprintf(fd, "# top edge value:\n");
        fprintf(fd, "ymin=%d\n", default_calibration.ymin);
	fprintf(fd, "# bottom edge value:\n");
        fprintf(fd, "ymax=%d\n", default_calibration.ymax);
        fprintf(fd, "\n");


        fclose(fd);
        return 1;
}

conf_data config_parse (void) {

	char file[MAXLEN];
	char input[MAXLEN], temp[MAXLEN];
	FILE *fd;
	size_t len;
	conf_data config = default_config;

	sprintf( file, "%s", CONFIG_FILE);
	if (!file_exists(file)) {
		if (!create_config_file(file)) {
			fprintf (stderr,"Failed to create default config file: %s\n", file);
			exit (1);
		}
	}

	fd = fopen (file, "r");
	if (fd == NULL) {
		fprintf (stderr,"Could not open configuration file: %s\n", file);
		exit (1);
	}

	while ((fgets (input, sizeof (input), fd)) != NULL) {

		if ((strncmp ("serial_device=", input, 14)) == 0) {
			strncpy (temp, input + 14,MAXLEN-1);
			len=strlen(temp);
			temp[len-1]='\0';
			sprintf(config.serial_device, "%s", temp);
			memset (temp, 0, sizeof (temp));
		}

		if ((strncmp ("uinput_device=", input, 14)) == 0) {
			strncpy (temp, input + 14,MAXLEN-1);
			len=strlen(temp);
			temp[len-1]='\0';
			sprintf(config.uinput_device, "%s", temp);
			memset (temp, 0, sizeof (temp));
		}

		if ((strncmp ("rightclick_enable=", input, 18)) == 0) {
			strncpy (temp, input + 18,MAXLEN-1);
			len=strlen(temp);
			temp[len+1]='\0';
			config.rightclick_enable = atoi(temp);
		}

		if ((strncmp ("rightclick_duration=", input, 20)) == 0) {
			strncpy (temp, input + 20,MAXLEN-1);
			len=strlen(temp);
			temp[len+1]='\0';
			config.rightclick_duration = atoi(temp);
		}

		if ((strncmp ("rightclick_range=", input, 17)) == 0) {
			strncpy (temp, input + 17,MAXLEN-1);
			len=strlen(temp);
			temp[len+1]='\0';
			config.rightclick_range = atoi(temp);
		}

	}

	fclose(fd);
	return config;
}

calibration_data calibration_parse (void) {

	char file[MAXLEN];
	char input[MAXLEN], temp[MAXLEN];
	FILE *fd;
	size_t len;
	calibration_data calibration = default_calibration;

	sprintf( file, "%s", CONFIG_FILE);
	if (!file_exists(file)) {
		if (!create_config_file(file)) {
			fprintf (stderr,"Failed to create default config file: %s\n", file);
			exit (1);
		}
	}

	fd = fopen (file, "r");
	if (fd == NULL) {
		fprintf (stderr,"Could not open configuration file: %s\n", file);
		exit (1);
	}

	while ((fgets (input, sizeof (input), fd)) != NULL) {

		if ((strncmp ("xmin=", input, 5)) == 0) {
			strncpy (temp, input + 5,MAXLEN-1);
			len=strlen(temp);
			temp[len+1]='\0';
			calibration.xmin = atoi(temp);
		}

		if ((strncmp ("xmax=", input, 5)) == 0) {
			strncpy (temp, input + 5,MAXLEN-1);
			len=strlen(temp);
			temp[len+1]='\0';
			calibration.xmax = atoi(temp);
		}

		if ((strncmp ("ymin=", input, 5)) == 0) {
			strncpy (temp, input + 5,MAXLEN-1);
			len=strlen(temp);
			temp[len+1]='\0';
			calibration.ymin = atoi(temp);
		}

		if ((strncmp ("ymax=", input, 5)) == 0) {
			strncpy (temp, input + 5,MAXLEN-1);
			len=strlen(temp);
			temp[len+1]='\0';
			calibration.ymax = atoi(temp);
		}
	}

	fclose(fd);
	return calibration;
}

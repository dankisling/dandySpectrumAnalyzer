//Raspberry Pi Spectrum Analyzer Controller
//The controller calls down the the driver to
//change its values and prompt reading samples
//Also completes all floating point calculations
//dandy, 2016, version 0.4

#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "gpu_fft/mailbox.h"
#include "gpu_fft/gpu_fft.h"
#include "specAn_driver_pi3.h"

#define DEFAULT_POW2_SAMPLES_PER_STEP	13
#define DEFAULT_SET_STEPS		10
#define DEFAULT_SET_DELAY		0
#define	DEFAULT_SET_AMP			(1 << 2)
#define DEFAULT_CORRECTION		-2000.0

#define FLAG_S	0
#define FLAG_P	1
#define FLAG_D	2
#define FLAG_A	3
#define FLAG_L	4
#define FLAG_C	5
#define FLAG_O	6

struct specAn_edit_values* edit;
float correction = DEFAULT_CORRECTION;

int mb = 0;
int fdrv = 0;
int lo_on = 1;
void* mmap_base;

float* freq_vals;

FILE* fout;

struct GPU_FFT_COMPLEX* base;
struct GPU_FFT* fft;

int initialize(void);
int setup_struct(void);
int setup_driver(void);
int read_samples(void);
int construct_spectrum(void);
void cleanup(void);

void parsearg(char*);
int check_flag(char);
void set_correction(char*);
void set_ampvals(char*);
void set_divisor(char*);
void set_samples(char*);
void set_steps(char*);
void set_localvals(char*);

int calculate_dac(int, float);
int calculate_oct(float);
float calculate_freq(int, int);


int main(int n_args, char** args)
{
	int ret = 0;
	int n = 0;
	
	printf("%d\n", n_args);

	for(n = 0; n < n_args; n++)
		printf("%s\n", args[n]);
		
	if(initialize() < 0)
	{
		ret = -1;
		goto cleanup;
	}
	
	setup_struct();
	
	if(n_args > 1)
	{
		int i = 1;
		for(i = 1; i < n_args; i++)
		{
			parsearg(args[i]);
		}
	}
	
	if(setup_driver() < 0)
	{
		ret = -2;
		goto cleanup;
	}
	
	if(read_samples() < 0)
	{
		ret = -3;
		goto cleanup;
	}
	
	if(construct_spectrum() < 0)
	{
		ret = -4;
		goto cleanup;
	}

cleanup:
	cleanup();
	return ret;
		
}

//Initializes the file structs, character device
//file and the default frequency values
int initialize(void)
{
	mb = mbox_open();
	fout = fopen("controller.log", "w+");

	if(!fout)
	{
		printf("Output file could not be opened\n");
		return -1;
	}

	fdrv = open("/dev/specAn_driver_pi3", O_RDWR);

	if(fdrv < 0)
	{
		printf("Character device failed to open: %d\n", fdrv);
		fclose(fout);
		return -1;
	}
	
	freq_vals = (float*)malloc(sizeof(float) * 10);
	
	*freq_vals = 360000;
	*(freq_vals + 1) = 370000;
	*(freq_vals + 2) = 380000;
	*(freq_vals + 3) = 390000;
	*(freq_vals + 4) = 400000;
	*(freq_vals + 5) = 410000;
	*(freq_vals + 6) = 420000;
	*(freq_vals + 7) = 430000;
	*(freq_vals + 8) = 440000;
	*(freq_vals + 9) = 450000;

        return 0;
}

//Sets the struct to be written to the driver to
//the dafault valules then waits to be edited from
//shell commands.
int setup_struct(void)
{
	int ret = 0;
	int i = 0;
	int length = sizeof(*edit) + 2 * DEFAULT_SET_STEPS * sizeof(uint16_t);
	edit = (struct specAn_edit_values*)malloc(length);

	edit->update = UPDATE_STEPS | UPDATE_SAMPLES_PER_STEP | UPDATE_DELAY | UPDATE_AMP_BITS | UPDATE_DAC_OCT;
	edit->steps = DEFAULT_SET_STEPS;
	edit->pow2_samples_per_step = DEFAULT_POW2_SAMPLES_PER_STEP;
	edit->delay = DEFAULT_SET_DELAY;
	edit->amp_vals = DEFAULT_SET_AMP;

	for(i = 0; i < DEFAULT_SET_STEPS; i++)
	{
		uint16_t oct = (uint16_t)calculate_oct(freq_vals[i] + correction);
		uint16_t dac = (uint16_t)calculate_dac(oct, freq_vals[i] + correction);
		edit->lo_values[i] = oct;
		edit->lo_values[i + DEFAULT_SET_STEPS] = dac;
	}
	
	return 0;
}

//Writes the values to be edited to the driver.
//Sets the fft struct to the correct size and memory
//maps the drivers buffer into user space
int setup_driver(void)
{
	int length = sizeof(struct specAn_edit_values) + 2 * edit->steps * sizeof(uint16_t);
	int ret = 0;
	
	ret = gpu_fft_prepare(mb, edit->pow2_samples_per_step, GPU_FFT_FWD, edit->steps, &fft);

	switch(ret)
        {
                case -1: printf("Unable to enable V3D. Please check your firmware is up to date.\n");return -1;
                case -2: printf("log2_N=%d not supported.  Try between 8 and 22.\n", edit->pow2_samples_per_step);return -1;
                case -3: printf("Out of memory.  Try a smaller batch or increase GPU memory.\n");return -1;
                case -4: printf("Unable to map Videocore peripherals into ARM memory space.\n");return -1;
                case -5: printf("Can't open libbcm_host.\n");return -1;
        }
        
        int size = (1 << edit->pow2_samples_per_step) * (int)edit->steps;
        mmap_base = mmap(0, size, PROT_READ, MAP_SHARED, fdrv, 0);
        
        ret = write(fdrv, (void*)edit, length);
        return ret;
}

//Prompts the driver to read the samples.  Formats these samples
//into their correct float values then places them in the fft
//struct for fft calculation
int read_samples(void)
{
	int i = 0;
	uint8_t* mmap_ptr;
	int step_size = (1 << edit->pow2_samples_per_step);

	read(fdrv, NULL, 0);

	for(i = 0; i < edit->steps; i++)
	{
		int j = 0;
		base = fft->in + i * fft->step;
		mmap_ptr = (uint8_t*)(mmap_base + i * step_size);

		for(j = 0; j < step_size; j++, mmap_ptr++)
		{
			float sample = (float)(*mmap_ptr) / 255.0 * 5.0;
			
			base[j].re = sample;
			base[j].im = 0;
		}
	}

	usleep(1);
	gpu_fft_execute(fft);

	return 0;
}

//After running the fft, a file is written for the GUI
//This file takes all fft results and reconstructs the spectrum
int construct_spectrum(void)
{
	int i = 0;
	float s_freq = 1000000.0 / (edit->delay + 1);
	float res =  s_freq / ((float)(1 << edit->pow2_samples_per_step));

	int start = (int)(450000.0 / res);
	int end = (int)(460000.0 / res);

	for(i = 0; i < edit->steps; i++)
	{
		int j = 0;
		base = fft->out + i * fft->step;

		for(j = start; j < end; j++)
		{
			float val = j * res - lo_on * freq_vals[i];
			float amp = sqrt(pow(base[j].re, 2) + pow(base[j].im, 2)) / s_freq;
			char out[32];
			int size = sprintf(out, "%0.6f %0.6f\n", val, amp);
			fwrite(out, 1, size, fout);
		}
	}

	return 0;
}

//Cleans up before exiting the program.  Responsible
//for memory freeing and releasing the fft struct
//from the videocore
void cleanup(void)
{
	int size = (1 << edit->pow2_samples_per_step) * edit->steps;
	fclose(fout);
	gpu_fft_release(fft);
	munmap(mmap_base, size);
	close(fdrv);
	free((void*)edit);
	free((void*)freq_vals);
}

//Responsible for parsing the incoming flags into values
//to write to the driver.  This functionality is accomplished
//across several helper functions
void parsearg(char* arg)
{
	char flag[3];
	char val[100];
	int index;
	
	strncpy(flag, arg, 2);
	if(flag[0] != '-')
	{
		printf("Invalid flag start, flags are declraed with '-'\n");
		return;
	}
	
	index = check_flag(flag[1]);
	
	if(index < 0)
	{
		printf("Invalid flag\n");
		return;
	}
	
	if(index == FLAG_O)
		goto setfns;
	
	if(arg[2] != '=')
	{
		printf("Flags must be followed by a data value to be set to, follow the flag with '=' and the value\n");
		return;
	}
	
	strcpy(val, arg + 3);
	
setfns:
	switch(index)
	{
		case FLAG_S:
			set_steps(val);
			return;
		case FLAG_P:
			set_samples(val);
			return;
		case FLAG_D:
			set_divisor(val);
			return;
		case FLAG_A:
			set_ampvals(val);
			return;
		case FLAG_L:
			set_localvals(val);
			return;
		case FLAG_C:
			set_correction(val);
			return;
		case FLAG_O:
			lo_on = 0;
			return;
	}
}

int check_flag(char f)
{
	switch(f)
	{
		case 's':
			return FLAG_S;
		case 'p':
			return FLAG_P;
		case 'd':
			return FLAG_D;
		case 'a':
			return FLAG_A;
		case 'l':
			return FLAG_L;
		case 'c':
			return FLAG_C;
		case 'o':
			return FLAG_O;
		default:
			return -1;
	}
}

void set_localvals(char* val)
{
	int i = 0;
	char* split = strtok(val, ",");
	freq_vals = realloc(freq_vals, sizeof(float)*edit->steps);
	
	for(i = 0; (i < edit->steps) && split != NULL; i++)
	{
		float freq = strtof(split, NULL);
		freq_vals[i] = freq;
		int oct = calculate_oct(freq + correction);
		int dac = calculate_dac(oct, freq + correction);
		
		edit->lo_values[i] = (uint16_t)oct;
		edit->lo_values[i + edit->steps] = (uint16_t)dac;
		
		split = strtok(NULL, ",");
	}
}

void set_correction(char* val)
{
	correction = strtof(val, NULL);
}

void set_ampvals(char* val)
{
	uint16_t ampvals = (uint16_t)strtoumax(val, NULL, 2);
	edit->amp_vals = (~ampvals) & 0x7;
	printf("DEBUG: %x becomes %x\n", ~ampvals, edit->amp_vals);
}

void set_divisor(char* val)
{
	uint16_t delay = (uint16_t)strtoumax(val, NULL, 10);
	edit->delay = delay - 1;
}

void set_samples(char* val)
{
	uint16_t samples = (uint16_t)strtoumax(val, NULL, 10);
	edit->pow2_samples_per_step = samples;
}

void set_steps(char* val)
{
	uint16_t steps = (uint16_t)strtoumax(val, NULL, 10);
	
	edit = realloc(edit, sizeof(struct specAn_edit_values) + 2 * steps * sizeof(uint16_t));
	edit->steps = steps;
}


//These final functions are to calculate oct and dac from the frequency and 
//the frequency from the oct and dac values on the VCO
int calculate_dac(int oct, float freq)
{
	return (int)(2048.5 - 2078.0 * ((float)(1 << (10 + oct))) / freq);
}

int calculate_oct(float freq)
{
	return (int)(3.322 * log10(freq / 1039.0));
}

float calculate_freq(int oct, int dac)
{
	return ((float)(1 << oct)) * 2078.0 / (2 - (float)dac / 1024.0);
}


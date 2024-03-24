#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef __3DS__
#include <3ds.h>
#endif

#include "clownmdemu/clownmdemu.h"

#ifdef __3DS__
 #undef CLOCKS_PER_SEC
 #define CLOCKS_PER_SEC 1000

 #undef clock
 #define clock osGetTime

 #undef clock_t
 #define clock_t u64
#endif

static const ClownMDEmu_Configuration clownmdemu_configuration = {
	{CLOWNMDEMU_REGION_OVERSEAS, CLOWNMDEMU_TV_STANDARD_NTSC},
	{cc_false, cc_false, {cc_false, cc_false}},
	{{cc_false, cc_false, cc_false, cc_false, cc_false, cc_false}, cc_false},
	{{cc_false, cc_false, cc_false}, cc_false}
};
static ClownMDEmu_Constant clownmdemu_constant;
static ClownMDEmu_State clownmdemu_state;
static const ClownMDEmu clownmdemu = CLOWNMDEMU_PARAMETERS_INITIALISE(&clownmdemu_configuration, &clownmdemu_constant, &clownmdemu_state);

static cc_s16l sample_buffer[CC_MAX(CC_MAX(CLOWNMDEMU_FM_SAMPLE_RATE_NTSC, CLOWNMDEMU_FM_SAMPLE_RATE_PAL) * 2, CC_MAX(CLOWNMDEMU_PSG_SAMPLE_RATE_NTSC, CLOWNMDEMU_PSG_SAMPLE_RATE_PAL))];

static unsigned char *rom;
static size_t rom_size;

static cc_u8f CartridgeRead(const void* const user_data, const cc_u32f address)
{
	(void)user_data;

	if (address < rom_size)
		return rom[address];
	else
		return 0;
}

static void CartridgeWritten(const void* const user_data, const cc_u32f address, const cc_u8f value)
{
	(void)user_data;
	(void)address;
	(void)value;
}

static void ColourUpdated(const void* const user_data, const cc_u16f index, const cc_u16f colour)
{
	(void)user_data;
	(void)index;
	(void)colour;
}

static void ScanlineRendered(const void* const user_data, const cc_u16f scanline, const cc_u8l* const pixels, const cc_u16f screen_width, const cc_u16f screen_height)
{
	(void)user_data;
	(void)scanline;
	(void)pixels;
	(void)screen_width;
	(void)screen_height;
}

static cc_bool InputRequested(const void* const user_data, const cc_u8f player_id, const ClownMDEmu_Button button_id)
{
	(void)user_data;
	(void)player_id;
	(void)button_id;

	return cc_false;
}

static void FMAudioToBeGenerated(const void* const user_data, const size_t total_frames, void (* const generate_fm_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_frames))
{
	(void)user_data;

	generate_fm_audio(&clownmdemu, sample_buffer, total_frames);
}

static void PSGAudioToBeGenerated(const void* const user_data, const size_t total_samples, void (* const generate_psg_audio)(const ClownMDEmu *clownmdemu, cc_s16l *sample_buffer, size_t total_samples))
{
	(void)user_data;

	generate_psg_audio(&clownmdemu, sample_buffer, total_samples);
}

static void CDSeeked(const void* const user_data, const cc_u32f sector_index)
{
	(void)user_data;
	(void)sector_index;
}

static const cc_u8l* CDSectorRead(const void* const user_data)
{
	static cc_u8l buffer[2048];

	(void)user_data;

	return buffer;
}

static cc_bool LoadFile(const char* const file_path, unsigned char** const file_buffer, size_t* const file_size)
{
	FILE* const file = fopen(file_path, "rb");

	if (file != NULL)
	{
		if (!fseek(file, 0, SEEK_END))
		{
			const long long_size = ftell(file);

			if (long_size != -1 && !fseek(file, 0, SEEK_SET))
			{
				*file_size = long_size;

				*file_buffer = (unsigned char*)malloc(*file_size);

				if (*file_buffer != NULL)
				{
					if (fread(*file_buffer, *file_size, 1, file) == 1)
					{
						fclose(file);
						return cc_true;
					}

					free(*file_buffer);
				}
			}
		}

		fclose(file);
	}

	return cc_false;
}

int main(const int argc, const char* const * const argv)
{
	int exit_code;

	exit_code = EXIT_FAILURE;

#ifdef __3DS__
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
#endif

	if (argc < 3)
	{
		fputs("Error: Must pass path to ROM file and number of iterations.\n", stderr);
	}
	else
	{
		const unsigned long total_iterations = strtoul(argv[2], NULL, 0);

		ClownMDEmu_Constant_Initialise(&clownmdemu_constant);

		if (!LoadFile(argv[1], &rom, &rom_size))
		{
			fputs("Error: Unable to read ROM file.\n", stderr);
		}
		else
		{
			double average_buffer[0x10];
			cc_u16f average_buffer_index, average_buffer_total;

			average_buffer_index = 0;
			average_buffer_total = 0;

			for (;;)
			{
				clock_t start_time;
				cc_u16f i;
				double time_taken, average;

				static const ClownMDEmu_Callbacks clownmdemu_callbacks = {
					NULL,
					CartridgeRead,
					CartridgeWritten,
					ColourUpdated,
					ScanlineRendered,
					InputRequested,
					FMAudioToBeGenerated,
					PSGAudioToBeGenerated,
					CDSeeked,
					CDSectorRead
				};

				ClownMDEmu_State_Initialise(&clownmdemu_state);
				ClownMDEmu_Reset(&clownmdemu, &clownmdemu_callbacks, cc_false);

				start_time = clock();

				for (i = 0; i < total_iterations; ++i)
					ClownMDEmu_Iterate(&clownmdemu, &clownmdemu_callbacks);

				time_taken = (double)(clock() - start_time) / CLOCKS_PER_SEC;
				average_buffer[average_buffer_index] = time_taken;
				average_buffer_index = (average_buffer_index + 1) % CC_COUNT_OF(average_buffer);
				average_buffer_total = CC_MIN(CC_COUNT_OF(average_buffer), average_buffer_total + 1);

				average = 0.0;

				for (i = 0; i < average_buffer_total; ++i)
					average += average_buffer[i];

				average /= average_buffer_total;

				fprintf(stdout, "Average time taken: %f\n", average);

			#ifdef __3DS__
				if (!aptMainLoop())
					break;

				hidScanInput();

				if (hidKeysHeld() != 0)
					break;
			#endif
			}

			free(rom);

			exit_code = EXIT_SUCCESS;
		}
	}

#ifdef __3DS__
	gfxExit();
#endif

	return exit_code;
}

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include <eZ80.h>
#include <eZ80F92.h>
#include <defines.h>

#include "mos-interface.h"
#include "vdp.h"
#include "agontimer.h"

extern void write16bit(UINT16 w);
extern void write32bit(UINT32 w);

typedef struct {
    uint8_t chip_type;
    uint8_t chip_variant;
    uint32_t clock;
    uint8_t n_channels;
    uint24_t f_scale;
    uint24_t loop_start;

    uint8_t header_size;
    uint24_t data_start;
    uint24_t data_length;

    bool loop_enabled;

    uint32_t gd3_location;

    float volume_multiplier;
	float stored_multiplier;
	
    bool pause;

    char header_data[64];
	char *song_data;
    uint16_t data_pointer;
}
vgm_info;

typedef struct {

    uint8_t vgm_file;
	vgm_info vgm_info;
	uint8_t state;

}
game_state;

game_state game;

void delay_ticks(UINT16 ticks_end) { //16.7ms ticks
	
	UINT32 ticks = 0;
	ticks_end *= 6;
	while(true) {
		
		waitvblank();
		ticks++;
		if(ticks >= ticks_end) break;
		
	}
	
}

void delay_cents(UINT16 ticks_end) { //100ms ticks
	
	UINT32 ticks = 0;
	ticks_end *= 6;
	while(true) {
		
		waitvblank();
		ticks++;
		if(ticks >= ticks_end) break;
		
	}
	
}

void delay_secs(UINT16 ticks_end) { //1 sec ticks
	
	UINT32 ticks = 0;
	ticks_end *= 60;
	while(true) {
		
		waitvblank();
		ticks++;
		if(ticks >= ticks_end) break;
		
	}
	
}

bool isKthBitSet(uint8_t data, uint8_t k) {
    return (data & (1 << k)) != 0;
}

void channel_ready(int channel) {

	waitvblank();
	while (isKthBitSet(getsysvar_audioSuccess(), channel)) continue;
		//if () break;
	
	
}

#define change_volume(channel, volume) \
    do { \
        putch(23); \
        putch(0); \
        putch(133); \
        putch((channel)); \
        putch(2); \
        putch((volume)); \
    } while (0)
	

/* void change_frequency(UINT8 channel, UINT16 frequency) {
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(101); //Non envelope but custom waveform
	write16bit(frequency);
	
} */

#define change_frequency(channel, frequency) \
    do { \
        putch(23); \
        putch(0); \
        putch(133); \
        putch((channel)); \
        putch(3); \
        write16bit((frequency)); \
    } while (0)


void play_simple(int channel, int vol, int duration, int frequency) {
	
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(0);
	putch(vol);
	
	write16bit(frequency);
	write16bit(duration);
	
}

void reset_channel(uint8_t channel) {
	
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(10); //Non envelope but custom waveform
	
}

void play_simple_force(int channel, int wavetype, int vol, int frequency) {
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(4); //Non envelope but custom waveform
	putch(wavetype);
	putch(vol);
	
	write16bit(frequency);
	write16bit(0);
	
}

void play_saw_force(int channel, int vol, int frequency) {
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(3); //Simple, play forever
	putch(vol);
	
	write16bit(frequency);
	write16bit(2000);
	
}

void play_advanced(int channel, int  attack, int decay, int sustain, int release, int wavetype, int peakvol, int duration, int frequency, int freq_end, int end_style) {
	
	channel_ready(channel);
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(2);
	putch(wavetype);
	putch(peakvol);

	write16bit(frequency);
	write16bit(duration);
	write16bit(attack);
	putch(sustain);
	write16bit(decay);
	write16bit(release);
	write16bit(freq_end);
	putch(end_style);
		
}

void play_advanced_keep(int channel, int peakvol, int duration, int frequency) {
	
	channel_ready(channel);
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(6);
	
	putch(peakvol);
	write16bit(frequency);
	write16bit(duration);
		
}

void play_sample(UINT8 channel, UINT8 sample_id, UINT8 volume) {
	
	channel_ready(channel);
	
	putch(23);
	putch(0);
	putch(133);
	
	putch(channel);
	putch(5);
	putch(sample_id);
	putch(1); //Playback mode
	
	putch(volume);
	
}

void load_wav(const char* filename, uint8_t sample_id) {
    
	int i, j, header_size, bit_rate;
	int8_t sample8;
	int16_t sample16;
	uint32_t data_size, sample_rate;
    unsigned char header[300];
	UINT32 remainder;
	char *sample_buffer;
    UINT8 fp;
	FIL *fo;
	
    fp = mos_fopen(filename, fa_read);
    if (!fp) {
        printf("Error: could not open file %s\n", filename);
        return;
    }

    for (i = 0; i < 300 && !mos_feof(fp); i++) {
        header[i] = mos_fgetc(fp);
        if (i >= 3 && header[i - 7] == 'd' && header[i - 6] == 'a' && header[i - 5] == 't' && header[i - 4] == 'a') {
			break; // found the start of the data
        }
    }

    if (i >= 300 || !(header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F') ||
        !(header[8] == 'W' && header[9] == 'A' && header[10] == 'V' && header[11] == 'E' &&
          header[12] == 'f' && header[13] == 'm' && header[14] == 't' && header[15] == ' ') ||
        !(header[16] == 0x10 && header[17] == 0x00 && header[18] == 0x00 && header[19] == 0x00 &&
          header[20] == 0x01 && header[21] == 0x00 && header[22] == 0x01 && header[23] == 0x00)) {
        printf("Error: invalid WAV file\n");
        mos_fclose(fp);
        return;
    }
	
    sample_rate = header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24);
	data_size = header[i - 3] | (header[i - 2] << 8) | (header[i - 1] << 16) | (header[i] << 24);

    if (header[34] == 8 && header[35] == 0) {
        bit_rate = 8;
    } else {
        printf("Error: unsupported PCM format\n");
        mos_fclose(fp);
        return;
    }
	
		//remainder = data_size % 5000;	
		printf("%u total samples.\r\n", data_size);

		//printf("Done reading.\r\n");
		putch(23);
		putch(0);
		putch(133);
		putch(0);
		putch(5); //Sample mode

		putch(sample_id); //Sample ID
		putch(0); //Record mode
		
		write32bit(data_size);
		//else if (bit_rate == 16) write32bit(data_size / 2);
		
		write16bit(sample_rate);
		
		sample_buffer = (char *) malloc(data_size);
		mos_fread(fp, sample_buffer, data_size);
		mos_puts(sample_buffer, data_size, 0);
		free(sample_buffer);

    mos_fclose(fp);
}

#define AY_3_8910_CLOCK_SPEED 1789773
//#define AY_FREQ 55930
#define AY_3_8910_CHANNELS 3

typedef struct {
    uint8_t volume;
    uint8_t frequency_coarse;
    uint8_t frequency_fine;
    int frequency_hz;
	bool enabled;
} ay_channel_state;

ay_channel_state ay_states[AY_3_8910_CHANNELS];

typedef struct {
	uint8_t latched_channel;
	uint8_t latched_type;
} sn_global;

sn_global sn;

typedef struct {
    // uint8_t volume_high;
	// uint8_t volume_low;
	uint16_t volume;
    uint8_t frequency_high;
    uint8_t frequency_low;
	uint16_t frequency_combined;
    uint16_t frequency_hz;
	bool enabled;
} sn_channel_state;

sn_channel_state sn_channels[6];
uint8_t sn_vol_table[16] = {127, 113, 101, 90, 80, 71, 63, 56, 50, 45, 40, 36, 32, 28, 25, 0};

void process_0x50_command(unsigned char data) {
    //uint8_t channel = 0;
    uint8_t type = 0;
    uint16_t value = 0;

    if (data & 0x80) { //Latch data

        sn.latched_channel = (data >> 5) & 0x03; // Bits 5-6 are the channel number
        sn.latched_type = (data >> 4) & 0x01; // Bit 4 is the type (1 = volume, 0 = frequency)

        if (sn.latched_type == 0) sn_channels[sn.latched_channel].frequency_low = data & 0x0F; // Bits 0-3 are the value

        else if (sn.latched_type == 1) sn_channels[sn.latched_channel].volume = sn_vol_table[data & 0x0F];

    } else { //Pure data

        if (sn.latched_type == 0) sn_channels[sn.latched_channel].frequency_high = data & 0x3F; // Bits 0-3 are the value
        else if (sn.latched_type == 1) sn_channels[sn.latched_channel].volume = sn_vol_table[data & 0x0F];

    }

    if (sn.latched_channel == 3) return; //Ignore the noise channel

    //For an SN76489 game.vgm_info.f_scale will be MASTER_CLOCK / 32.

    sn_channels[sn.latched_channel].frequency_hz = game.vgm_info.f_scale / ((sn_channels[sn.latched_channel].frequency_high << 4) | (sn_channels[sn.latched_channel].frequency_low) & 0x0F);

    if (sn.latched_type == 0) change_frequency(sn.latched_channel, sn_channels[sn.latched_channel].frequency_hz);

    else if (sn.latched_type == 1) change_volume(sn.latched_channel, sn_channels[sn.latched_channel].volume * game.vgm_info.volume_multiplier);

}

uint8_t ay_vol_table[16] = {0, 8, 17, 25, 34, 42, 51, 59, 68, 76, 85, 93, 102, 110, 119, 127};
uint8_t ym_vol_table[32] = {0, 1, 1, 1, 2, 2, 2, 3, 4, 4, 5, 6, 7, 8, 10, 11, 13, 15, 17, 20, 23, 26, 30, 35, 39, 45, 51, 59, 67, 76, 86, 98};

UINT32 little_long(UINT8 b0, UINT8 b1, UINT8 b2, UINT8 b3) {

	UINT32 little_long = 0;
		
	little_long += (UINT32)(b0 << 24);
	little_long += (UINT32)(b1 << 16);
	little_long += (UINT32)(b2 << 8);
	little_long += (UINT32)b3;
		
	return little_long;
		
}

uint32_t bigtolittle32(const uint8_t *bytes) {
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

uint32_t littletobig32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0]) |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

uint32_t l2b(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    return ((uint32_t)b0 << 24) |
           ((uint32_t)b1 << 16) |
           ((uint32_t)b2 << 8) |
           (uint32_t)b3;
}

enum VgmParserState {
    READ_COMMAND,
    WAIT_SAMPLES,
    WRITE_REGISTER,
    END_OF_SOUND_DATA
};

enum VgmParserState state = READ_COMMAND;
unsigned int start_timer0 = 0, target_timer0 = 0;
uint24_t delay_length = 0;
uint24_t samples_to_wait = 0;

UINT8 vgm_init(UINT8 fp) {

    uint8_t vgm_min_header[64];
    uint8_t vgm_rem_header[192]; //Up to, likely less.
    uint8_t i;
    uint32_t test_clock = 0;
    game.vgm_info.data_length = 0;
    game.vgm_info.data_start = 0;

    game.vgm_info.data_pointer = 0;
    game.vgm_info.loop_start = 0;
    game.vgm_info.gd3_location = 0;
    game.vgm_info.header_size = 0;

    game.vgm_info.pause = false;

    state = READ_COMMAND;

    mos_fread(fp, game.vgm_info.header_data, 64);

    if (game.vgm_info.header_data[0] != 'V' || game.vgm_info.header_data[1] != 'g' || game.vgm_info.header_data[2] != 'm') return 0;

    if (game.vgm_info.header_data[0x34] != 0) { //Is there more header than the classic 64 bytes?

        game.vgm_info.header_size = * (uint32_t * )(game.vgm_info.header_data + 0x34) - 0xC; // (less the 12 bytes between 0x34 and 0x40)
        mos_fread(fp, game.vgm_info.header_data + 64, game.vgm_info.header_size);

    }

    game.vgm_info.data_length = * (uint32_t * )(game.vgm_info.header_data + 0x04) - 0x04 - game.vgm_info.header_size - 0x40;
    game.vgm_info.data_start = game.vgm_info.header_size + 0x40;
	
	game.vgm_info.song_data = (char*)malloc(game.vgm_info.data_length);

    if ( * (uint32_t * )(game.vgm_info.header_data + 0x0C) != 0) { //SN76489?

        game.vgm_info.chip_type = 1;
        game.vgm_info.clock = * (uint32_t * )(game.vgm_info.header_data + 0x0C);
        game.vgm_info.n_channels = 3;

    } else if ( * (uint32_t * )(game.vgm_info.header_data + 0x74) != 0) { //AY-3-8910?

        game.vgm_info.chip_type = 0;
        game.vgm_info.clock = * (uint32_t * )(game.vgm_info.header_data + 0x74);

        game.vgm_info.chip_variant = game.vgm_info.header_data[0x78];

        game.vgm_info.n_channels = 3;

    } else if ( * (uint32_t * )(game.vgm_info.header_data + 0x84) != 0) { //NES APU?

        game.vgm_info.chip_type = 3;
        game.vgm_info.clock = * (uint32_t * )(game.vgm_info.header_data + 0x84);

        game.vgm_info.n_channels = 3;

    }

    if ( * (uint32_t * )(game.vgm_info.header_data + 0x1C) != 0) { //Loop detected

        game.vgm_info.loop_start = * (uint32_t * )(game.vgm_info.header_data + 0x1C);
        //game.vgm_info.loop_enabled = true;

    } else game.vgm_info.loop_start = 0;

    if (game.vgm_info.header_data[0x0C] == 0x80) {

        printf("Detected a dual chip VGM, exiting.\r\n");
        return 0;

    }

    if ( * (uint32_t * )(game.vgm_info.header_data + 0x14) != 0) {

        game.vgm_info.gd3_location = * (uint32_t * )(game.vgm_info.header_data + 0x14) + 0x14;

    }

    mos_flseek(fp, game.vgm_info.data_start);

    game.vgm_info.data_pointer = 0;
	
	mos_fread(fp, game.vgm_info.song_data, game.vgm_info.data_length);

    //timer0_begin(104, 4); //44100Hz
	//timer0_begin(1044, 4); //44100 / 10 = 44100Hz
    timer0_begin(313, 4); //4410Hz / 3 = 14700Hz
    //timer0_begin(1567, 4); //44100Hz / 15
    //timer0_begin(2089, 4); //44100Hz / 20
	//timer0_begin(3134, 4); //44100Hz / 30

    if (game.vgm_info.chip_type == 0) { //AY

        for (i = 0; i < game.vgm_info.n_channels; i++) {

            ay_states[i].volume = 0xFF;
            ay_states[i].frequency_coarse = 0xFF;
            ay_states[i].frequency_fine = 0xFF;
            ay_states[i].frequency_hz = 0;
            ay_states[i].enabled = 0;

			//reset_channel(i);
            play_simple(i, 0, 10, 0); //Mute

        }

        if (game.vgm_info.chip_variant == 0x10) game.vgm_info.f_scale = game.vgm_info.clock / 32;
        else game.vgm_info.f_scale = game.vgm_info.clock / 16;
    } else if (game.vgm_info.chip_type == 1) { //SN

        for (i = 0; i < game.vgm_info.n_channels; i++) {

            sn_channels[i].volume = 0;
            sn_channels[i].frequency_high = 0x00;
            sn_channels[i].frequency_low = 0x00;
            sn_channels[i].frequency_hz = 0;
            sn_channels[i].enabled = 0;

            //reset_channel(i);
			play_simple(i, 0, 10, 0); //Mute

        }
        game.vgm_info.f_scale = game.vgm_info.clock / 32;
    }

    mos_fclose(fp);
	return 1;

}

void vgm_loop_init() {

    uint8_t i;

    timer0_end();
    timer0_begin(313, 4); //4410Hz / 3 = 14700Hz

    for (i = 0; i < game.vgm_info.n_channels + 1; i++) {

        printf("Setting up channel %u\r\n", i);

        ay_states[i].volume = 0xFF;
        ay_states[i].frequency_coarse = 0xFF;
        ay_states[i].frequency_fine = 0xFF;
        ay_states[i].frequency_hz = 0;
        ay_states[i].enabled = 0;

        //reset_channel(i);
		play_simple(i, 0, 10, 0); //Mute

    }

}

void vgm_cleanup(UINT8 fp) {
    uint8_t i;

	free(game.vgm_info.song_data);
    mos_fclose(fp);
    for (i = 0; i < game.vgm_info.n_channels; i++) {
        play_simple(i, 0, 1, 0); //Mute
		change_volume(i, 0);
		//reset_channel(i);
    }

    start_timer0 = 0;
    target_timer0 = 0;
    delay_length = 0;
    samples_to_wait = 0;
    timer0_end();

}

/* UINT8 parse_vgm_file(UINT8 fp) {  //FILE BASED VERSION
    unsigned char byte, firstbyte, secondbyte;
    unsigned short reg = 0;
    unsigned char value = 0;
	
    //enum VgmParserState state = READ_COMMAND;

    //while (state != END_OF_SOUND_DATA) {
        switch (state) {
			printf("State: %u", state);
            case READ_COMMAND:
				//byte = mos_fgetc(fp);
                if (byte != EOF) {
                    switch (byte) {
                        case 0x61: // wait n samples
                            firstbyte = mos_fgetc(fp);
							secondbyte = mos_fgetc(fp);
							//samples_to_wait = (mos_fgetc(fp) << 8) | mos_fgetc(fp);
							samples_to_wait = (secondbyte << 8) | firstbyte;
							start_timer0 = timer0;
                            state = WAIT_SAMPLES;
							//printf("Delay command: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
							//printf("D ");
							//printf("DELAY 0x61: 0x%02X 0x%02X (%u) samples (timer0=%u).\r\n", firstbyte, secondbyte, samples_to_wait, start_timer0);
							return 0;
                            break;
                        case 0x62: // wait 735 samples (1/60 second)
                            samples_to_wait = 735;
							start_timer0 = timer0;
                            state = WAIT_SAMPLES;
							//printf("Delay command: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
							//printf("D ");
							//printf("DELAY 0x62: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
                            return 0;
							break;
                        case 0x63: // wait 882 samples (1/50 second)
                            samples_to_wait = 882;
							start_timer0 = timer0;
                            state = WAIT_SAMPLES;
							//printf("DELAY 0x63: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
							//printf("Delay command: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
							//printf("D ");
                            return 0;
							break;
                        case 0x70: // delay n+1 samples
                        case 0x71:
                        case 0x72:
                        case 0x73:
                        case 0x74:
                        case 0x75:
                        case 0x76:
                        case 0x77:
                        case 0x78:
                        case 0x79:
                        case 0x7A:
                        case 0x7B:
                        case 0x7C:
                        case 0x7D:
                        case 0x7E:
                        case 0x7F:
                            samples_to_wait = (byte & 0x0F) + 1;
							start_timer0 = timer0;
                            state = WAIT_SAMPLES;
							//printf("DELAY 0x7n: %u samples (timer0=%u).\r\n", samples_to_wait, start_timer0);
							//printf("D ");
                            return 0;
							break;
                        case 0x50:
							value = mos_fgetc(fp);
							//process_0x50_command(value);
							return 0;
						case 0xA0: // write to registry
                            //reg = fread(&byte, 1, fp);
							reg = mos_fgetc(fp);
                            value = mos_fgetc(fp);
                            //ay_write_register(reg, value);
							//printf("WRITE: %u value %u.\r\n", reg, value);
							//printf("W ");
							//updateAY38910(reg, value);
							process_0xA0_command(reg,value);
                            return 0;
							break;
                        case 0x66: // end of sound data
                            state = END_OF_SOUND_DATA;
                            printf("\r\nEND OF DATA\r\n");
							// if (vgm_info.loop_start) {
								// 
								// state = READ_COMMAND;
								// vgm_cleanup(fp);
								// vgm_loop_init(fp);
								// 
								// 
							// } else return 1;
					
							return 1;
                        default:
                            // unsupported command, ignore and continue
                            return 0;
							break;
                    }
                }
                break;
            case WAIT_SAMPLES:
				target_timer0 = start_timer0 + samples_to_wait;    
				if (timer0 >= target_timer0) {
                    state = READ_COMMAND;
                    samples_to_wait = 0;
                }
                return 0;
				break;
            default:
                // invalid state, exit loop
                state = END_OF_SOUND_DATA;
                return 1;
        }
	//}
} */

UINT8 parse_vgm_file() {
    uint8_t byte, firstbyte, secondbyte;
    uint8_t reg = 0;
    uint8_t value = 0;

    //printf("%u\r\n", fo->fptr);
    if (game.vgm_file == NULL) return 1;
	
	switch (state) {

    case WAIT_SAMPLES:
        //target_timer0 = start_timer0 + samples_to_wait;		
        if (timer0 >= target_timer0) {
            state = READ_COMMAND;
            samples_to_wait = 0;
        }
        return 0;

    case READ_COMMAND:
        //byte = mos_fgetc(fp);
		byte = game.vgm_info.song_data[game.vgm_info.data_pointer++];
        if (byte != EOF) {
            switch (byte) {
            case 0x61: // wait n samples
                //firstbyte = mos_fgetc(fp);
				firstbyte = game.vgm_info.song_data[game.vgm_info.data_pointer++];
                //secondbyte = mos_fgetc(fp);
				secondbyte = game.vgm_info.song_data[game.vgm_info.data_pointer++];
                target_timer0 = timer0 + (secondbyte << 8) | firstbyte;
                //samples_to_wait = (secondbyte << 8) | firstbyte;
                //start_timer0 = timer0;
                state = WAIT_SAMPLES;
                return 0;
            case 0x62: // wait 735 samples (1/60 second)
                target_timer0 = timer0 + 735;
                state = WAIT_SAMPLES;
                return 0;
            case 0x63: // wait 882 samples (1/50 second)
                target_timer0 = timer0 + 882;
                state = WAIT_SAMPLES;
                return 0;
            case 0x70: // delay n+1 samples
            case 0x71:
            case 0x72:
            case 0x73:
            case 0x74:
            case 0x75:
            case 0x76:
            case 0x77:
            case 0x78:
            case 0x79:
            case 0x7A:
            case 0x7B:
            case 0x7C:
            case 0x7D:
            case 0x7E:
            case 0x7F:
                target_timer0 = timer0 + (byte & 0x0F) + 1;
                state = WAIT_SAMPLES;
                return 0;
            // case 0xB4:
                // reg = mos_fgetc(fp);
				// value = mos_fgetc(fp);
                // process_0xB4_command(reg, value);
                // return 0;            
			case 0x50:
                //value = mos_fgetc(fp);
				value = game.vgm_info.song_data[game.vgm_info.data_pointer++];
                process_0x50_command(value);
                return 0;
            case 0xA0: // AY_3_8910 command
                //process_0xA0_command(mos_fgetc(fp),mos_fgetc(fp));
                //switch (mos_fgetc(fp)) {
				switch (game.vgm_info.song_data[game.vgm_info.data_pointer++]) {
                case 0x06:
                    return 0; //We're not doing noises.
                case 0x00:
                    //ay_states[0].frequency_fine = mos_fgetc(fp);
					ay_states[0].frequency_fine = game.vgm_info.song_data[game.vgm_info.data_pointer++];
                    ay_states[0].frequency_hz = game.vgm_info.f_scale / (ay_states[0].frequency_coarse << 8 | ay_states[0].frequency_fine);
                    if (ay_states[0].enabled) change_frequency(0, ay_states[0].frequency_hz);
                    return 0;
                case 0x01:
                    ay_states[0].frequency_coarse = game.vgm_info.song_data[game.vgm_info.data_pointer++];
                    ay_states[0].frequency_hz = game.vgm_info.f_scale / (ay_states[0].frequency_coarse << 8 | ay_states[0].frequency_fine);
                    if (ay_states[0].enabled) change_frequency(0, ay_states[0].frequency_hz);
                    return 0;
                case 0x08:
                    ay_states[0].volume = ay_vol_table[(game.vgm_info.song_data[game.vgm_info.data_pointer++] & 0x0F)];
                    if (ay_states[0].enabled) change_volume(0, ay_states[0].volume * game.vgm_info.volume_multiplier);
                    return 0;
                case 0x09:
                    ay_states[1].volume = ay_vol_table[(game.vgm_info.song_data[game.vgm_info.data_pointer++] & 0x0F)];
                    if (ay_states[1].enabled) change_volume(1, ay_states[1].volume * game.vgm_info.volume_multiplier);
                    return 0;
                case 0x0A:
                    ay_states[2].volume = ay_vol_table[(game.vgm_info.song_data[game.vgm_info.data_pointer++] & 0x0F)];
                    if (ay_states[2].enabled) change_volume(2, ay_states[2].volume * game.vgm_info.volume_multiplier);
                    return 0;
                case 0x02:
                    ay_states[1].frequency_fine = game.vgm_info.song_data[game.vgm_info.data_pointer++];
                    ay_states[1].frequency_hz = game.vgm_info.f_scale / (ay_states[1].frequency_coarse << 8 | ay_states[1].frequency_fine);
                    if (ay_states[1].enabled) change_frequency(1, ay_states[1].frequency_hz);
                    return 0;
                case 0x03:
                    ay_states[1].frequency_coarse = game.vgm_info.song_data[game.vgm_info.data_pointer++];
                    ay_states[1].frequency_hz = game.vgm_info.f_scale / (ay_states[1].frequency_coarse << 8 | ay_states[1].frequency_fine);
                    if (ay_states[1].enabled) change_frequency(1, ay_states[1].frequency_hz);
                    return 0;
                case 0x04:
                    ay_states[2].frequency_fine = game.vgm_info.song_data[game.vgm_info.data_pointer++];
                    ay_states[2].frequency_hz = game.vgm_info.f_scale / (ay_states[2].frequency_coarse << 8 | ay_states[2].frequency_fine);
                    if (ay_states[2].enabled) change_frequency(2, ay_states[2].frequency_hz);
                    return 0;
                case 0x05:
                    ay_states[2].frequency_coarse = game.vgm_info.song_data[game.vgm_info.data_pointer++];
                    ay_states[2].frequency_hz = game.vgm_info.f_scale / (ay_states[2].frequency_coarse << 8 | ay_states[2].frequency_fine);
                    if (ay_states[2].enabled) change_frequency(2, ay_states[2].frequency_hz);
                    return 0;
                case 0x07:
                    value = game.vgm_info.song_data[game.vgm_info.data_pointer++];
                    ay_states[0].enabled = !(value & 0x01);
                    ay_states[1].enabled = !(value & 0x02);
                    ay_states[2].enabled = !(value & 0x04);

                    change_volume(0, !!ay_states[0].enabled * ay_states[0].volume * game.vgm_info.volume_multiplier);
                    change_volume(1, !!ay_states[1].enabled * ay_states[1].volume * game.vgm_info.volume_multiplier);
                    change_volume(2, !!ay_states[2].enabled * ay_states[2].volume * game.vgm_info.volume_multiplier);
                    return 0;

                default:
                    // Invalid register, ignore
                    return 0;
                }
                return 0;
            case 0x66: // end of sound data
                state = END_OF_SOUND_DATA;

                if (game.vgm_info.loop_start && game.vgm_info.loop_enabled) {

                    //mos_flseek(fp, game.vgm_info.loop_start);
                    game.vgm_info.data_pointer = game.vgm_info.loop_start;
					timer0_end();
					timer0_begin(313, 4); //4410Hz / 3 = 14700Hz
                    state = READ_COMMAND;
                    return 0;

                } else vgm_cleanup(game.vgm_file);

                return 1;
            default:
                // unsupported command, ignore and continue
                return 0;
                break;
            }
        }
        return 0;
    default:
        // invalid state, exit loop
        state = END_OF_SOUND_DATA;
        return 1;
    }
    return 0;
}

int main(int argc, char * argv[]) {
	
	int i = 0, j = 0;
	UINT24 t = 0;
	uint8_t key = 0, keycount;
	FIL *fo;
	
// 	if (argc < 2) {
		
	//	printf("Usage: play [song.vgm]");
		//return 0;
		
	//}
		
	game.vgm_info.loop_enabled = false;
	//for (i = 0; i < argc; i++) printf("- argv[%d]: %s\n\r", i, argv[i]);
	if (!strcmp(argv[2], "loop=true")) game.vgm_info.loop_enabled = true;
	else game.vgm_info.loop_enabled = false;
	
	game.vgm_file = mos_fopen(argv[1], fa_read);
    if (!game.vgm_file) { printf("Error: could not open file %s.\r\n", argv[1]); return 0; }
	
	if (vgm_init(game.vgm_file) == 0) return 0;
	
	game.vgm_info.volume_multiplier = 0.2;
	//printf("Size = %u", fo->obj.objsize);
	
	while (parse_vgm_file() != 1);
	
	vgm_cleanup(game.vgm_file); 

	return 0;
	
}


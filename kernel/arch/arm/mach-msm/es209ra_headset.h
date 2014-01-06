#ifndef LINUX_ES209RA_AUDIO_JACK
#define LINUX_ES209RA_AUDIO_JACK

struct es209ra_headset_platform_data {
	const char * keypad_name;
	unsigned int gpio_detout;
	unsigned int gpio_detin;
	unsigned int wait_time;
};

void es209ra_audio_jack_button_handler(int key);

#endif

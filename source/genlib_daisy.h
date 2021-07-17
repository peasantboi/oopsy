#ifndef GENLIB_DAISY_H
#define GENLIB_DAISY_H

#include "daisy.h"
#include "genlib.h"
#include "genlib_ops.h"
#include "genlib_exportfunctions.h"
#include <math.h>
#include <string>
#include <cstring> // memset
#include <stdarg.h> // vprintf

//#define OOPSY_USE_LOGGING 1

#if defined(OOPSY_TARGET_PATCH)
	#include "daisy_patch.h"
	#define OOPSY_IO_COUNT (4)
	typedef daisy::DaisyPatch Daisy;

#elif defined(OOPSY_TARGET_FIELD)
	#include "daisy_field.h"
	#define OOPSY_IO_COUNT (2)
	typedef daisy::DaisyField Daisy;

#elif defined(OOPSY_TARGET_PETAL)
	#include "daisy_petal.h"
	#define OOPSY_IO_COUNT (2)
	typedef daisy::DaisyPetal Daisy;

#elif defined(OOPSY_TARGET_POD)
	#include "daisy_pod.h"
	#define OOPSY_IO_COUNT (2)
	typedef daisy::DaisyPod Daisy;

#elif defined(OOPSY_TARGET_VERSIO)
	#include "daisy_versio.h"
	#define OOPSY_IO_COUNT (2)
	typedef daisy::DaisyVersio Daisy;

#elif defined(OOPSY_TARGET_TERRARIUM)
	#include "daisy_terrarium.h"
	#define OOPSY_IO_COUNT (2)
	typedef daisy::DaisyTerrarium Daisy;

#else
	#include "daisy_seed.h"
	#define OOPSY_IO_COUNT (2)
	typedef daisy::DaisySeed Daisy;

#endif

////////////////////////// DAISY EXPORT INTERFACING //////////////////////////

#define OOPSY_BUFFER_SIZE 48
#define OOPSY_MIDI_BUFFER_SIZE 256
#define OOPSY_LONG_PRESS_MS 250
#define OOPSY_DISPLAY_PERIOD_MS 10
#define OOPSY_SCOPE_MAX_ZOOM (8)
static const uint32_t OOPSY_SRAM_SIZE = 512 * 1024;
static const uint32_t OOPSY_SDRAM_SIZE = 64 * 1024 * 1024;

namespace oopsy {

	uint32_t sram_used = 0, sram_usable = 0;
	uint32_t sdram_used = 0, sdram_usable = 0;
	char * sram_pool = nullptr;
	char DSY_SDRAM_BSS sdram_pool[OOPSY_SDRAM_SIZE];

	void init() {
		if (!sram_pool) sram_pool = (char *)malloc(OOPSY_SRAM_SIZE);
		sram_usable = OOPSY_SRAM_SIZE;
		sram_used = 0;
		sdram_usable = OOPSY_SDRAM_SIZE;
		sdram_used = 0;
	}

	void * allocate(uint32_t size) {
		if (size < sram_usable) {
			void * p = sram_pool + sram_used;
			sram_used += size;
			sram_usable -= size;
			return p;
		} else if (size < sdram_usable) {
			void * p = sdram_pool + sdram_used;
			sdram_used += size;
			sdram_usable -= size;
			return p;
		}
		return nullptr;
	}

	void memset(void *p, int c, long size) {
		char *p2 = (char *)p;
		int i;
		for (i = 0; i < size; i++, p2++) *p2 = char(c);
	}

	// void genlib_memcpy(void *dst, const void *src, long size) {
	// 	char *s2 = (char *)src;
	// 	char *d2 = (char *)dst;
	// 	int i;
	// 	for (i = 0; i < size; i++, s2++, d2++)
	// 		*d2 = *s2;
	// }

	// void test() {
	// 	// memory test:
	// 	size_t allocated = 0;
	// 	size_t sz = 256;
	// 	int i;
	// 	while (sz < 515) {
	// 		sz++;
	// 		void * m = malloc(sz * 1024);
	// 		if (!m) break;
	// 		free(m);
	// 		log("%d: malloced %dk", i, sz);
	// 		i++;
	// 	}
	// 	log("all OK");
	// }

	struct Timer {
		int32_t period = OOPSY_DISPLAY_PERIOD_MS,
				t = OOPSY_DISPLAY_PERIOD_MS;

		bool ready(int32_t dt) {
			t += dt;
			if (t > period) {
				t = 0;
				return true;
			}
			return false;
		}
	};

	struct AppDef {
		const char * name;
		void (*load)();
	};
	typedef enum {
		MODE_NONE = 0,
		#ifdef OOPSY_MULTI_APP
		MODE_MENU,
		#endif

		#ifdef OOPSY_TARGET_HAS_OLED
		MODE_CONSOLE,
		#ifdef OOPSY_HAS_PARAM_VIEW
		MODE_PARAMS,
		#endif
		MODE_SCOPE,
		#endif

		MODE_COUNT
	} Mode;

	struct GenDaisy {

		Daisy hardware;
		AppDef * appdefs = nullptr;

		int mode, mode_default;
		int app_count = 1, app_selected = 0, app_selecting = 0;
		int menu_button_held = 0, menu_button_released = 0, menu_button_held_ms = 0, menu_button_incr = 0;
		int is_mode_selecting = 0;
		int param_count = 0;
		#ifdef OOPSY_HAS_PARAM_VIEW
		int param_selected = 0, param_is_tweaking = 0, param_scroll = 0;
		#endif

		uint32_t t = 0, dt = 10, frames = 0;
		Timer uitimer;

		// microseconds spent in audio callback
		float audioCpuUs = 0;

		float samplerate; // default 48014
		size_t blocksize; // default 48

		void (*mainloopCallback)(uint32_t t, uint32_t dt);
		void (*displayCallback)(uint32_t t, uint32_t dt);
		#ifdef OOPSY_HAS_PARAM_VIEW
		void (*paramCallback)(int idx, char * label, int len, bool tweak);
		#endif
		void * app = nullptr;
		void * gen = nullptr;
		bool nullAudioCallbackRunning = false;

		#ifdef OOPSY_TARGET_HAS_OLED

		enum {
			SCOPESTYLE_OVERLAY = 0,
			SCOPESTYLE_TOPBOTTOM,
			SCOPESTYLE_LEFTRIGHT,
			SCOPESTYLE_LISSAJOUS,
			SCOPESTYLE_COUNT
		} ScopeStyles;

		enum {
			SCOPEOPTION_STYLE = 0,
			SCOPEOPTION_SOURCE,
			SCOPEOPTION_ZOOM,
			SCOPEOPTION_COUNT
		} ScopeOptions;

		FontDef& font = Font_6x8;
		uint_fast8_t scope_zoom = 7;
		uint_fast8_t scope_step = 0;
		uint_fast8_t scope_option = 0, scope_style = SCOPESTYLE_TOPBOTTOM, scope_source = OOPSY_IO_COUNT/2;
		uint16_t console_cols, console_rows, console_line;
		char * console_stats;
		char * console_memory;
		char ** console_lines;
		float scope_data[SSD1309_WIDTH*2][2]; // 128 pixels
		char scope_label[11];
		#endif // OOPSY_TARGET_HAS_OLED

		#ifdef OOPSY_TARGET_USES_MIDI_UART
		daisy::UartHandler uart;
		uint8_t midi_in_written = 0, midi_out_written = 0;
		uint8_t midi_in_active = 0, midi_out_active = 0;
		uint8_t midi_out_data[OOPSY_MIDI_BUFFER_SIZE];
		float midi_in_data[OOPSY_BUFFER_SIZE];
		int midi_data_idx = 0;
		int midi_parse_state = 0;
		#endif //OOPSY_TARGET_USES_MIDI_UART

		template<typename A>
		void reset(A& newapp) {
			// first, remove callbacks:
			mainloopCallback = nullMainloopCallback;
			displayCallback = nullMainloopCallback;
			nullAudioCallbackRunning = false;
			hardware.ChangeAudioCallback(nullAudioCallback);
			while (!nullAudioCallbackRunning) dsy_system_delay(1);
			// reset memory
			oopsy::init();
			// install new app:
			app = &newapp;
			newapp.init(*this);
			// install new callbacks:
			mainloopCallback = newapp.staticMainloopCallback;
			displayCallback = newapp.staticDisplayCallback;
			#ifdef OOPSY_HAS_PARAM_VIEW
			paramCallback = newapp.staticParamCallback;
			#endif
			hardware.ChangeAudioCallback(newapp.staticAudioCallback);
			log("gen~ %s", appdefs[app_selected].name);
			log("SR %dkHz / %dHz", (int)(samplerate/1000), (int)hardware.seed.AudioCallbackRate());
			log("%d/%dK + %d/%dM", oopsy::sram_used/1024, OOPSY_SRAM_SIZE/1024, oopsy::sdram_used/1048576, OOPSY_SDRAM_SIZE/1048576);

			// reset some state:
			menu_button_incr = 0;
			#ifdef OOPSY_TARGET_USES_MIDI_UART
			midi_data_idx = 0;
			midi_in_written = 0, midi_out_written = 0;
			midi_in_active = 0, midi_out_active = 0;
			frames = 0;
			#endif
		}

		int run(AppDef * appdefs, int count, daisy::SaiHandle::Config::SampleRate SR) {
			this->appdefs = appdefs;
			app_count = count;

			mode_default = (Mode)(MODE_COUNT-1);
			mode = mode_default;

			hardware.Init();
			hardware.seed.SetAudioSampleRate(SR);
			samplerate = hardware.seed.AudioSampleRate();
			blocksize = hardware.seed.AudioBlockSize();  // default 48

			#ifdef OOPSY_USE_LOGGING
			hardware.seed.StartLog(true);
			//daisy::Logger<daisy::LOGGER_INTERNAL>::StartLog(false);
			#endif

			#ifdef OOPSY_TARGET_HAS_OLED
			console_cols = SSD1309_WIDTH / font.FontWidth + 1; // +1 to accommodate null terminators.
			console_rows = SSD1309_HEIGHT / font.FontHeight;
			console_memory = (char *)calloc(console_cols, console_rows);
			console_stats = (char *)calloc(console_cols, 1);
			for (int i=0; i<console_rows; i++) {
				console_lines[i] = &console_memory[i*console_cols];
			}
			console_line = console_rows-1;
			#endif

			hardware.StartAdc();
			hardware.StartAudio(nullAudioCallback);
			mainloopCallback = nullMainloopCallback;
			displayCallback = nullMainloopCallback;

			#ifdef OOPSY_TARGET_USES_MIDI_UART
			midi_data_idx = 0;
			midi_in_written = 0, midi_out_written = 0;
			midi_in_active = 0, midi_out_active = 0;
			uart.Init();
			uart.StartRx();
			#endif

			app_selected = 0;
			appdefs[app_selected].load();

			#ifdef OOPSY_TARGET_HAS_OLED
			console_display();
			#endif

			static bool blink;
			while(1) {
				uint32_t t1 = dsy_system_getnow();
				dt = t1-t;
				t = t1;

				// pulse seed LED for status according to CPU usage:
				hardware.seed.SetLed((t % 1000)/10 <= uint32_t(0.0001f*audioCpuUs*samplerate/blocksize));

				// handle app-level code (e.g. for CV/gate outs)
				mainloopCallback(t, dt);
				#ifdef OOPSY_TARGET_USES_MIDI_UART
				if (midi_out_written) {
					//log("midi %d", midi_out_written);
					//for (int i=0; i<midi_out_written; i+=3) log("%d %d %d", midi_out_data[i], midi_out_data[i+1], midi_out_data[i+2]);
				}
				{
					// input:
					while(uart.Readable()) {
						uint8_t byte = uart.PopRx();
						//log("midi in %d", byte);
						// Oopsy-level handling here
						#ifdef OOPSY_MULTI_APP
						if (byte >= 128 && byte < 240) {
							// status byte:
							midi_parse_state = (byte/16)-7;
						} else if (midi_parse_state == 5) {
							midi_parse_state = 0;
							// program change => load a new app
							app_selected = app_selecting = byte % app_count;
							appdefs[app_selected].load();
							continue;
						} else {
							// ignored:
							midi_parse_state = 0;
						}
						#endif // OOPSY_MULTI_APP

						// gen~ level handling here:
						if (midi_in_written < OOPSY_BUFFER_SIZE) {
							// scale (0, 255) to (0.0, 1.0)
							// to protect hardware from accidental patching
							midi_in_data[midi_in_written] = byte / 256.0f;
							midi_in_written++;
						}
						midi_in_active = 1;
					}
					// output:
					if (midi_out_written) {
						midi_out_active = 1;
						if (0 == uart.PollTx(midi_out_data, midi_out_written)) {
							midi_out_written = 0;
						}
					}
				}
				#endif

				if (uitimer.ready(dt)) {
					#ifdef OOPSY_USE_LOGGING
					//hardware.seed.PrintLine("helo %d", t);
					hardware.seed.PrintLine("the time is"FLT_FMT3"", FLT_VAR3(t/1000.f));
					#endif

					if (menu_button_held_ms > OOPSY_LONG_PRESS_MS) {
						// LONG PRESS
						is_mode_selecting = 1;
					}

					// CLEAR DISPLAY
					#ifdef OOPSY_TARGET_HAS_OLED
					hardware.display.Fill(false);
					#endif
					#if (OOPSY_TARGET_PETAL || OOPSY_TARGET_TERRARIUM)
					hardware.ClearLeds();
					#endif // (OOPSY_TARGET_PETAL || OOPSY_TARGET_TERRARIUM)

					#ifdef OOPSY_TARGET_PETAL
					// has no mode selection
					is_mode_selecting = 0;
					#if defined(OOPSY_MULTI_APP)
					// multi-app is always in menu mode:
					mode = MODE_MENU;
					#endif
					for(int i = 0; i < 8; i++) {
						float white = (i == app_selecting || menu_button_released);
						hardware.SetRingLed((daisy::DaisyPetal::RingLed)i,
							(i == app_selected || white) * 1.f,
							white * 1.f,
							(i < app_count) * 0.3f + white * 1.f
						);
					}
					#endif //OOPSY_TARGET_PETAL

					#ifdef OOPSY_TARGET_VERSIO
					// has no mode selection
					is_mode_selecting = 0;
					#if defined(OOPSY_MULTI_APP)
					// multi-app is always in menu mode:
					mode = MODE_MENU;
					#endif
					for(int i = 0; i < 4; i++) {
						float white = (i == app_selecting || menu_button_released);
						hardware.SetLed(i,
							(i == app_selected || white) * 1.f,
							white * 1.f,
							(i < app_count) * 0.3f + white * 1.f
						);
					}
					#endif //OOPSY_TARGET_VERSIO

					// Handle encoder increment actions:
					if (is_mode_selecting) {
						mode += menu_button_incr;
						if (mode >= MODE_COUNT) mode = 0;
						if (mode < 0) mode = MODE_COUNT-1;
					#ifdef OOPSY_MULTI_APP
					} else if (mode == MODE_MENU) {
						#ifdef OOPSY_TARGET_VERSIO
						app_selecting = menu_button_incr;
						#else
						app_selecting += menu_button_incr;
						#endif
						if (app_selecting >= app_count) app_selecting -= app_count;
						if (app_selecting < 0) app_selecting += app_count;
					#endif // OOPSY_MULTI_APP
					#ifdef OOPSY_TARGET_HAS_OLED
					} else if (mode == MODE_SCOPE) {
						switch (scope_option) {
							case SCOPEOPTION_STYLE: {
								scope_style = (scope_style + menu_button_incr) % SCOPESTYLE_COUNT;
							} break;
							case SCOPEOPTION_SOURCE: {
								scope_source = (scope_source + menu_button_incr) % (OOPSY_IO_COUNT*2);
							} break;
							case SCOPEOPTION_ZOOM: {
								scope_zoom = (scope_zoom + menu_button_incr) % OOPSY_SCOPE_MAX_ZOOM;
							} break;
						}
					#ifdef OOPSY_HAS_PARAM_VIEW
					} else if (mode == MODE_PARAMS) {
						if (!param_is_tweaking) {
							param_selected += menu_button_incr;
							if (param_selected >= param_count) param_selected = 0;
							if (param_selected < 0) param_selected = param_count-1;
						}
					#endif //OOPSY_HAS_PARAM_VIEW
					#endif //OOPSY_TARGET_HAS_OLED
					}

					// SHORT PRESS
					if (menu_button_released) {
						menu_button_released = 0;
						if (is_mode_selecting) {
							is_mode_selecting = 0;
						#ifdef OOPSY_MULTI_APP
						} else if (mode == MODE_MENU) {
							if (app_selected != app_selecting) {
								app_selected = app_selecting;
								#ifndef OOPSY_TARGET_HAS_OLED
								mode = mode_default;
								#endif
								appdefs[app_selected].load();
								continue;
							}
						#endif
						#ifdef OOPSY_TARGET_HAS_OLED
						} else if (mode == MODE_SCOPE) {
							scope_option = (scope_option + 1) % SCOPEOPTION_COUNT;
						#ifdef OOPSY_HAS_PARAM_VIEW
						} else if (mode == MODE_PARAMS) {
							param_is_tweaking = !param_is_tweaking;
						#endif //OOPSY_HAS_PARAM_VIEW
						#endif //OOPSY_TARGET_HAS_OLED
						}
					}

					// OLED DISPLAY:
					#ifdef OOPSY_TARGET_HAS_OLED
					int showstats = 0;
					switch(mode) {
						#ifdef OOPSY_MULTI_APP
						case MODE_MENU: {
							showstats = 1;
							for (int i=0; i<console_rows; i++) {
								if (i == app_selecting) {
									hardware.display.SetCursor(0, font.FontHeight * i);
									hardware.display.WriteString((char *)">", font, true);
								}
								if (i < app_count) {
									hardware.display.SetCursor(font.FontWidth, font.FontHeight * i);
									hardware.display.WriteString((char *)appdefs[i].name, font, i != app_selected);
								}
							}
						} break;
						#endif //OOPSY_MULTI_APP
						#ifdef OOPSY_HAS_PARAM_VIEW
						case MODE_PARAMS: {
							char label[console_cols+1];
							// ensure selected parameter is on-screen:
							if (param_scroll > param_selected) param_scroll = param_selected;
							if (param_scroll < (param_selected - console_rows + 1)) param_scroll = (param_selected - console_rows + 1);
							int idx = param_scroll; // offset this for screen-scroll
							for (int line=0; line<console_rows && idx < param_count; line++, idx++) {
								paramCallback(idx, label, console_cols, param_is_tweaking && idx == param_selected);
								hardware.display.SetCursor(0, font.FontHeight * line);
								hardware.display.WriteString(label, font, (param_selected != idx));
							}
						} break;
						#endif // OOPSY_HAS_PARAM_VIEW
						case MODE_SCOPE: {
							showstats = 1;
							uint8_t h = SSD1309_HEIGHT;
							uint8_t w2 = SSD1309_WIDTH/2, w4 = SSD1309_WIDTH/4;
							uint8_t h2 = h/2, h4 = h/4;
							size_t zoomlevel = scope_samples();
							hardware.display.Fill(false);

							// stereo views:
							switch (scope_style) {
							case SCOPESTYLE_OVERLAY: {
								// stereo overlay:
								for (uint_fast8_t i=0; i<SSD1309_WIDTH; i++) {
									int j = i*2;
									hardware.display.DrawLine(i, (1.f-scope_data[j][0])*h2, i, (1.f-scope_data[j+1][0])*h2, 1);
									hardware.display.DrawLine(i, (1.f-scope_data[j][1])*h2, i, (1.f-scope_data[j+1][1])*h2, 1);
								}
							} break;
							case SCOPESTYLE_TOPBOTTOM:
							{
								// stereo top-bottom
								for (uint_fast8_t i=0; i<SSD1309_WIDTH; i++) {
									int j = i*2;
									hardware.display.DrawLine(i, (1.f-scope_data[j][0])*h4, i, (1.f-scope_data[j+1][0])*h4, 1);
									hardware.display.DrawLine(i, (1.f-scope_data[j][1])*h4+h2, i, (1.f-scope_data[j+1][1])*h4+h2, 1);
								}
							} break;
							case SCOPESTYLE_LEFTRIGHT:
							{
								// stereo L/R:
								for (uint_fast8_t i=0; i<w2; i++) {
									int j = i*4;
									hardware.display.DrawLine(i, (1.f-scope_data[j][0])*h2, i, (1.f-scope_data[j+1][0])*h2, 1);
									hardware.display.DrawLine(i + w2, (1.f-scope_data[j][1])*h2, i + w2, (1.f-scope_data[j+1][1])*h2, 1);
								}
							} break;
							default:
							{
								for (uint_fast8_t i=0; i<SSD1309_WIDTH; i++) {
									int j = i*2;
									hardware.display.DrawPixel(
										w2 + h2*scope_data[j][0],
										h2 + h2*scope_data[j][1],
										1
									);
								}

								// for (uint_fast8_t i=0; i<SSD1309_WIDTH; i++) {
								// 	int j = i*2;
								// 	hardware.display.DrawLine(
								// 		w2 + h2*scope_data[j][0],
								// 		h2 + h2*scope_data[j][1],
								// 		w2 + h2*scope_data[j+1][0],
								// 		h2 + h2*scope_data[j+1][1],
								// 		1
								// 	);
								// }
							} break;
							} // switch

							// labelling:
							switch (scope_option) {
								case SCOPEOPTION_SOURCE: {
									hardware.display.SetCursor(0, h - font.FontHeight);
									switch(scope_source) {
									#if (OOPSY_IO_COUNT == 4)
										case 0: hardware.display.WriteString("in1  in2", font, true); break;
										case 1: hardware.display.WriteString("in3  in4", font, true); break;
										case 2: hardware.display.WriteString("out1 out2", font, true); break;
										case 3: hardware.display.WriteString("out3 out4", font, true); break;
										case 4: hardware.display.WriteString("in1  out1", font, true); break;
										case 5: hardware.display.WriteString("in2  out2", font, true); break;
										case 6: hardware.display.WriteString("in3  out3", font, true); break;
										case 7: hardware.display.WriteString("in4  out4", font, true); break;
									#else
										case 0: hardware.display.WriteString("in1  in2", font, true); break;
										case 1: hardware.display.WriteString("out1 out2", font, true); break;
										case 2: hardware.display.WriteString("in1  out1", font, true); break;
										case 3: hardware.display.WriteString("in2  out2", font, true); break;
									#endif
									}
								} break;
								case SCOPEOPTION_ZOOM: {
									// each pixel is zoom samples; zoom/samplerate seconds
									float scope_duration = SSD1309_WIDTH*(1000.f*zoomlevel/samplerate);
									int offset = snprintf(scope_label, console_cols, "%dx %dms", zoomlevel, (int)ceilf(scope_duration));
									hardware.display.SetCursor(0, h - font.FontHeight);
									hardware.display.WriteString(scope_label, font, true);
								} break;
								// for view style, just leave it blank :-)
							}
						} break;
						case MODE_CONSOLE:
						{
							showstats = 1;
							console_display();
							break;
						}
						default: {
						}
					}
					if (is_mode_selecting) {
						hardware.display.DrawRect(0, 0, SSD1309_WIDTH-1, SSD1309_HEIGHT-1, 1);
					}
					if (showstats) {
						int offset = 0;
						#ifdef OOPSY_TARGET_USES_MIDI_UART
						offset += snprintf(console_stats+offset, console_cols-offset, "%c%c", midi_in_active ? '<' : ' ', midi_out_active ? '>' : ' ');
						midi_in_active = midi_out_active = 0;
						#endif
						offset += snprintf(console_stats+offset, console_cols-offset, "%02d%%", int(0.0001f*audioCpuUs*(samplerate)/blocksize));
						// stats:
						hardware.display.SetCursor(SSD1309_WIDTH - (offset) * font.FontWidth, font.FontHeight * 0);
						hardware.display.WriteString(console_stats, font, true);
					}
					#endif //OOPSY_TARGET_HAS_OLED

					menu_button_incr = 0;

					// handle app-level code (e.g. for LED etc.)
					displayCallback(t, dt);

					#ifdef OOPSY_TARGET_HAS_OLED
					hardware.display.Update();
					#endif //OOPSY_TARGET_HAS_OLED
					#if (OOPSY_TARGET_PETAL || OOPSY_TARGET_VERSIO || OOPSY_TARGET_TERRARIUM)
					hardware.UpdateLeds();
					#endif //(OOPSY_TARGET_PETAL || OOPSY_TARGET_VERSIO || OOPSY_TARGET_TERRARIUM)


				} // uitimer.ready

			}
			return 0;
		}

		void audio_preperform(size_t size) {
			#ifdef OOPSY_TARGET_USES_MIDI_UART
			// fill remainder of midi buffer with non-data:
			for (size_t i=midi_in_written; i<size; i++) midi_in_data[i] = -0.1f;
			// done with midi input:
			midi_in_written = 0;
			#endif

            /*
			#if (OOPSY_TARGET_FIELD || OOPSY_TARGET_VERSIO)
				hardware.ProcessAnalogControls();
				#if OOPSY_TARGET_FIELD
				hardware.UpdateDigitalControls();
				#endif
			#else
				hardware.DebounceControls();
				hardware.UpdateAnalogControls();
			#endif
            */
            hardware.ProcessAllControls();

			#ifdef OOPSY_TARGET_FIELD
			menu_button_held = hardware.GetSwitch(0)->Pressed();
			menu_button_incr += hardware.GetSwitch(1)->FallingEdge();
			menu_button_held_ms = hardware.GetSwitch(0)->TimeHeldMs();
			if (hardware.GetSwitch(0)->FallingEdge()) menu_button_released = 1;
			#elif OOPSY_TARGET_VERSIO
			// menu_button_held = hardware.tap_.Pressed();
			// menu_button_incr += hardware.GetKnobValue(6) * app_count;
			// menu_button_held_ms = hardware.tap_.TimeHeldMs();
			// if (hardware.tap_.FallingEdge()) menu_button_released = 1;
			#elif OOPSY_TARGET_TERRARIUM
				/* Do nothing, no encuder exists */
			#else
			menu_button_held = hardware.encoder.Pressed();
			menu_button_incr += hardware.encoder.Increment();
			menu_button_held_ms = hardware.encoder.TimeHeldMs();
			if (hardware.encoder.FallingEdge()) menu_button_released = 1;
			#endif
		}

		void audio_postperform(float **buffers, size_t size) {
			#ifdef OOPSY_TARGET_HAS_OLED
			if (mode == MODE_SCOPE) {
				// selector for scope storage source:
				// e.g. for OOPSY_IO_COUNT=4, inputs:outputs as 0123:4567 makes:
				// 01, 23, 45, 67  2n:2n+1  i1i2 i3i4 o1o2 o3o4
				// 04, 15, 26, 37  n:n+ch   i1o1 i2o2 i3o3 i4o4
				int n = scope_source % OOPSY_IO_COUNT;
				float * buf0 = (scope_source < OOPSY_IO_COUNT) ? buffers[2*n  ] : buffers[n   ];
				float * buf1 = (scope_source < OOPSY_IO_COUNT) ? buffers[2*n+1] : buffers[n+OOPSY_IO_COUNT];

				// float * buf0 = scope_source ? buffers[0] : buffers[2];
				// float * buf1 = scope_source ? buffers[1] : buffers[3];
				size_t samples = scope_samples();
				for (size_t i=0; i<size/samples; i++) {
					float min0 = 10.f, max0 = -10.f;
					float min1 = 10.f, max1 = -10.f;
					for (size_t j=0; j<samples; j++) {
						float pt0 = *buf0++;
						float pt1 = *buf1++;
						min0 = min0 > pt0 ? pt0 : min0;
						max0 = max0 < pt0 ? pt0 : max0;
						min1 = min1 > pt1 ? pt1 : min1;
						max1 = max1 < pt1 ? pt1 : max1;
					}
					scope_data[scope_step][0] = (min0);
					scope_data[scope_step][1] = (min1);
					scope_step++;
					scope_data[scope_step][0] = (max0);
					scope_data[scope_step][1] = (max1);
					scope_step++;
					if (scope_step >= SSD1309_WIDTH*2) scope_step = 0;
				}
			}
			#endif
			#if (OOPSY_TARGET_POD)
				hardware.UpdateLeds();
			#endif
			frames++;
		}

		#ifdef OOPSY_TARGET_HAS_OLED
		inline int scope_samples() {
			// valid zoom sizes: 1, 2, 3, 4, 6, 8, 12, 16, 24
			switch(scope_zoom) {
				case 1: case 2: case 3: case 4: return scope_zoom; break;
				case 5: return 6; break;
				case 6: return 12; break;
				case 7: return 16; break;
				default: return 24; break;
			}
		}

		GenDaisy& console_display() {
			for (int i=0; i<console_rows; i++) {
				hardware.display.SetCursor(0, font.FontHeight * i);
				hardware.display.WriteString(console_lines[(i+console_line) % console_rows], font, true);
			}
			return *this;
		}
		#endif // OOPSY_TARGET_HAS_OLED

		GenDaisy& log(const char * fmt, ...) {
			#ifdef OOPSY_TARGET_HAS_OLED
			va_list argptr;
			va_start(argptr, fmt);
			vsnprintf(console_lines[console_line], console_cols, fmt, argptr);
			va_end(argptr);
			console_line = (console_line + 1) % console_rows;
			#endif
			return *this;
		}

		#if OOPSY_TARGET_USES_MIDI_UART
		void midi_postperform(float * buf, size_t size) {
			for (size_t i=0; i<size && midi_out_written+1 < OOPSY_MIDI_BUFFER_SIZE; i++) {
				if (buf[i] >= 0.f) {
					// scale (0.0, 1.0) back to (0, 255) for MIDI bytes
					midi_out_data[midi_out_written] = buf[i] * 256.0f;
					midi_out_written++;
				}
			}
		}

		void midi_message3(uint8_t status, uint8_t b1, uint8_t b2) {
			if (midi_out_written+3 < OOPSY_MIDI_BUFFER_SIZE) {
				midi_out_data[midi_out_written] = status;
				midi_out_data[midi_out_written+1] = b1;
				midi_out_data[midi_out_written+2] = b2;
				midi_out_written += 3;
			}
		}

		void midi_nullData(Data& data) {
			for (int i=0; i<data.dim; i++) {
				data.write(-1, i, 0);
			}
		}

		void midi_fromData(Data& data) {
			double b = data.read(midi_data_idx, 0);
			while (b >= 0. && midi_out_written < OOPSY_MIDI_BUFFER_SIZE-1) {
				// erase it from [data midi]
				data.write(-1, midi_data_idx, 0);
				// write it to our active outbuffer:
				midi_out_data[midi_out_written++] = b;
				// and advance one index in the [data midi]
				midi_data_idx++; if (midi_data_idx >= data.dim) midi_data_idx = 0;
				b = data.read(midi_data_idx, 0);
			}
		}
		#endif //OOPSY_TARGET_USES_MIDI_UART

		#if (OOPSY_TARGET_FIELD)
		void setFieldLedsFromData(Data& data) {
			for(long i = 0; i < daisy::DaisyField::LED_LAST && i < data.dim; i++) {
				// LED indices run A1..8, B8..1, Knob1..8
				// switch here to re-order the B8-1 to B1-8
				long idx=i;
				if (idx > 7 && idx < 16) idx = 23-i;
				hardware.led_driver.SetLed(idx, data.read(i, 0));
			}
			hardware.led_driver.SwapBuffersAndTransmit();
		};
		#endif

		static void nullAudioCallback(float **hardware_ins, float **hardware_outs, size_t size);

		static void nullMainloopCallback(uint32_t t, uint32_t dt) {}
	} daisy;

	void GenDaisy::nullAudioCallback(float **hardware_ins, float **hardware_outs, size_t size) {
		daisy.nullAudioCallbackRunning = true;
		// zero audio outs:
		for (int i=0; i<OOPSY_IO_COUNT; i++) {
			memset(hardware_outs[i], 0, sizeof(float)*size);
		}
	}


	// Curiously-recurring template to make App definitions simpler:
	template<typename T>
	struct App {

		static void staticMainloopCallback(uint32_t t, uint32_t dt) {
			T& self = *(T *)daisy.app;
			self.mainloopCallback(daisy, t, dt);
		}

		static void staticDisplayCallback(uint32_t t, uint32_t dt) {
			T& self = *(T *)daisy.app;
			self.displayCallback(daisy, t, dt);
		}

		static void staticAudioCallback(float **hardware_ins, float **hardware_outs, size_t size) {
			uint32_t start = dsy_tim_get_tick(); // 200MHz
			daisy.audio_preperform(size);
			((T *)daisy.app)->audioCallback(daisy, hardware_ins, hardware_outs, size);
			#if (OOPSY_IO_COUNT == 4)
			float * buffers[] = {hardware_ins[0], hardware_ins[1], hardware_ins[2], hardware_ins[3], hardware_outs[0], hardware_outs[1], hardware_outs[2], hardware_outs[3]};
			#else
			float * buffers[] = {hardware_ins[0], hardware_ins[1], hardware_outs[0], hardware_outs[1]};
			#endif
			daisy.audio_postperform(buffers, size);
			// convert elapsed time (200Mhz ticks) to CPU percentage (with a slew to capture fluctuations)
			daisy.audioCpuUs += 0.03f*(((dsy_tim_get_tick() - start) / 200.f) - daisy.audioCpuUs);
		}

		#ifdef OOPSY_HAS_PARAM_VIEW
		static void staticParamCallback(int idx, char * label, int len, bool tweak) {
			T& self = *(T *)daisy.app;
			self.paramCallback(daisy, idx, label, len, tweak);
		}
		#endif //OOPSY_HAS_PARAM_VIEW
	};

}; // oopsy::

void genlib_report_error(const char *s) { oopsy::daisy.log(s); }
void genlib_report_message(const char *s) { oopsy::daisy.log(s); }

unsigned long genlib_ticks() { return dsy_system_getnow(); }

t_ptr genlib_sysmem_newptr(t_ptr_size size) {
	return (t_ptr)oopsy::allocate(size);
}

t_ptr genlib_sysmem_newptrclear(t_ptr_size size) {
	t_ptr p = genlib_sysmem_newptr(size);
	if (p) oopsy::memset(p, 0, size);
	return p;
}


#endif //GENLIB_DAISY_H

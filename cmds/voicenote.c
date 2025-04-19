// /**
//  * voicenote.c - A shell command that converts spoken audio to text files
//  * 
//  * Compile with:
//  * gcc -o voicenote voicenote.c -lportaudio -lpocketsphinx -lsphinxbase -lpthread -lm
//  */

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <signal.h>
// #include <portaudio.h>
// #include <pocketsphinx/pocketsphinx.h>

// #define SAMPLE_RATE 16000
// #define FRAMES_PER_BUFFER 512
// #define NUM_CHANNELS 1
// #define PA_SAMPLE_TYPE paInt16
// #define SAMPLE_SILENCE 0
// #define MAX_RECORDING_SECONDS 60

// /* Define the path to the model files */
// #define MODELDIR "/usr/local/share/pocketsphinx/model"
// #define ACOUSTICMODEL MODELDIR "/en-us/en-us"
// #define LANGUAGEMODEL MODELDIR "/en-us/en-us.lm.bin"
// #define DICTIONARY MODELDIR "/en-us/cmudict-en-us.dict"

// typedef struct {
//     int16_t *recorded_samples;
//     int frame_index;
//     int max_frame_index;
//     volatile int is_recording;
// } paTestData;

// paTestData data;

// /* This function will be called by PortAudio when audio is available */
// static int recordCallback(const void *inputBuffer, void *outputBuffer,
//                           unsigned long framesPerBuffer,
//                           const PaStreamCallbackTimeInfo* timeInfo,
//                           PaStreamCallbackFlags statusFlags,
//                           void *userData) {
//     paTestData *data = (paTestData*)userData;
//     const int16_t *rptr = (const int16_t*)inputBuffer;
//     int16_t *wptr = &data->recorded_samples[data->frame_index * NUM_CHANNELS];
//     long framesToCalc;
//     unsigned long i;

//     if (!data->is_recording) {
//         return paComplete;
//     }

//     framesToCalc = framesPerBuffer;

//     if (data->frame_index + framesToCalc > data->max_frame_index) {
//         framesToCalc = data->max_frame_index - data->frame_index;
//         if (framesToCalc <= 0) {
//             return paComplete;
//         }
//     }

//     if (inputBuffer == NULL) {
//         for (i = 0; i < framesToCalc; i++) {
//             *wptr++ = SAMPLE_SILENCE;  /* left */
//             if (NUM_CHANNELS == 2) *wptr++ = SAMPLE_SILENCE;  /* right */
//         }
//     } else {
//         for (i = 0; i < framesToCalc; i++) {
//             *wptr++ = *rptr++;  /* left */
//             if (NUM_CHANNELS == 2) *wptr++ = *rptr++;  /* right */
//         }
//     }

//     data->frame_index += framesToCalc;
//     return paContinue;
// }

// void handle_sigint(int sig) {
//     data.is_recording = 0;
//     printf("\nStopping recording...\n");
// }

// void save_audio_to_file(const char *filename, paTestData *data) {
//     FILE *fp = fopen(filename, "wb");
//     if (!fp) {
//         fprintf(stderr, "Could not open file %s for writing\n", filename);
//         return;
//     }

//     // Write WAV header
//     // RIFF header
//     fprintf(fp, "RIFF");
//     int data_size = data->frame_index * NUM_CHANNELS * sizeof(int16_t);
//     int file_size = 36 + data_size;
//     fwrite(&file_size, sizeof(int), 1, fp);
//     fprintf(fp, "WAVE");

//     // fmt chunk
//     fprintf(fp, "fmt ");
//     int chunk_size = 16;
//     fwrite(&chunk_size, sizeof(int), 1, fp);
//     short audio_format = 1; // PCM
//     fwrite(&audio_format, sizeof(short), 1, fp);
//     fwrite(&NUM_CHANNELS, sizeof(short), 1, fp);
//     fwrite(&SAMPLE_RATE, sizeof(int), 1, fp);
//     int byte_rate = SAMPLE_RATE * NUM_CHANNELS * sizeof(int16_t);
//     fwrite(&byte_rate, sizeof(int), 1, fp);
//     short block_align = NUM_CHANNELS * sizeof(int16_t);
//     fwrite(&block_align, sizeof(short), 1, fp);
//     short bits_per_sample = 16;
//     fwrite(&bits_per_sample, sizeof(short), 1, fp);

//     // data chunk
//     fprintf(fp, "data");
//     fwrite(&data_size, sizeof(int), 1, fp);
//     fwrite(data->recorded_samples, sizeof(int16_t), data->frame_index * NUM_CHANNELS, fp);

//     fclose(fp);
//     printf("Audio saved to %s\n", filename);
// }

// char* recognize_speech(const char* audio_file) {
//     ps_decoder_t *ps;
//     cmd_ln_t *config;
//     FILE *fh;
//     char const *hyp, *uttid;
//     int16_t buf[512];
//     int rv;
//     size_t nsamp;
    
//     // Initialize PocketSphinx configuration
//     config = cmd_ln_init(NULL, ps_args(), TRUE,
//                          "-hmm", ACOUSTICMODEL,
//                          "-lm", LANGUAGEMODEL,
//                          "-dict", DICTIONARY,
//                          NULL);
                         
//     if (config == NULL) {
//         fprintf(stderr, "Failed to create config object\n");
//         return NULL;
//     }
    
//     ps = ps_init(config);
//     if (ps == NULL) {
//         fprintf(stderr, "Failed to create recognizer\n");
//         cmd_ln_free_r(config);
//         return NULL;
//     }
    
//     fh = fopen(audio_file, "rb");
//     if (fh == NULL) {
//         fprintf(stderr, "Unable to open input file %s\n", audio_file);
//         ps_free(ps);
//         cmd_ln_free_r(config);
//         return NULL;
//     }
    
//     // Skip WAV header (44 bytes)
//     fseek(fh, 44, SEEK_SET);
    
//     ps_start_utt(ps);
//     while ((nsamp = fread(buf, sizeof(int16_t), 512, fh)) > 0) {
//         ps_process_raw(ps, buf, nsamp, FALSE, FALSE);
//     }
//     ps_end_utt(ps);
    
//     hyp = ps_get_hyp(ps, NULL);
    
//     char* result = NULL;
//     if (hyp != NULL) {
//         result = strdup(hyp);
//         printf("Recognized: %s\n", result);
//     } else {
//         printf("No speech could be recognized\n");
//     }
    
//     fclose(fh);
//     ps_free(ps);
//     cmd_ln_free_r(config);
    
//     return result;
// }

// void print_usage() {
//     printf("Usage: voicenote [-f filename] [-t seconds] [-d directory]\n");
//     printf("  -f filename   : Specify output text filename (default: note.txt)\n");
//     printf("  -t seconds    : Specify recording duration in seconds (default: 10, max: 60)\n");
//     printf("  -d directory  : Specify directory to save the note (default: current directory)\n");
//     printf("  Press Ctrl+C to stop recording before the time limit\n");
// }

// int main(int argc, char *argv[]) {
//     char *filename = "note.txt";
//     int recording_seconds = 10;
//     char *directory = ".";
//     char full_path[1024];
//     char temp_audio_file[1024];
//     PaStream *stream;
//     PaError err;
//     int opt;
    
//     // Parse command line options
//     while ((opt = getopt(argc, argv, "f:t:d:h")) != -1) {
//         switch (opt) {
//             case 'f':
//                 filename = optarg;
//                 break;
//             case 't':
//                 recording_seconds = atoi(optarg);
//                 if (recording_seconds <= 0 || recording_seconds > MAX_RECORDING_SECONDS) {
//                     fprintf(stderr, "Recording time must be between 1 and %d seconds\n", 
//                             MAX_RECORDING_SECONDS);
//                     return 1;
//                 }
//                 break;
//             case 'd':
//                 directory = optarg;
//                 break;
//             case 'h':
//                 print_usage();
//                 return 0;
//             default:
//                 print_usage();
//                 return 1;
//         }
//     }
    
//     // Create full path for output file
//     snprintf(full_path, sizeof(full_path), "%s/%s", directory, filename);
    
//     // Create temp audio file path
//     snprintf(temp_audio_file, sizeof(temp_audio_file), "%s/voicenote_temp.wav", directory);
    
//     // Initialize PortAudio
//     err = Pa_Initialize();
//     if (err != paNoError) {
//         fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
//         return 1;
//     }
    
//     // Initialize recording data
//     data.max_frame_index = recording_seconds * SAMPLE_RATE;
//     data.frame_index = 0;
//     data.is_recording = 1;
//     data.recorded_samples = (int16_t*)malloc(data.max_frame_index * NUM_CHANNELS * sizeof(int16_t));
//     if (!data.recorded_samples) {
//         fprintf(stderr, "Could not allocate record array\n");
//         Pa_Terminate();
//         return 1;
//     }
    
//     // Set up signal handler for Ctrl+C
//     signal(SIGINT, handle_sigint);
    
//     // Open audio stream
//     err = Pa_OpenDefaultStream(&stream,
//                               NUM_CHANNELS,        // input channels
//                               0,                   // output channels
//                               PA_SAMPLE_TYPE,      // sample format
//                               SAMPLE_RATE,
//                               FRAMES_PER_BUFFER,   // frames per buffer
//                               recordCallback,
//                               &data);
//     if (err != paNoError) {
//         fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
//         free(data.recorded_samples);
//         Pa_Terminate();
//         return 1;
//     }
    
//     // Start recording
//     err = Pa_StartStream(stream);
//     if (err != paNoError) {
//         fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
//         free(data.recorded_samples);
//         Pa_CloseStream(stream);
//         Pa_Terminate();
//         return 1;
//     }
    
//     printf("Recording... Press Ctrl+C to stop (max %d seconds)\n", recording_seconds);
    
//     // Wait for recording to complete
//     while ((err = Pa_IsStreamActive(stream)) == 1 && data.is_recording) {
//         Pa_Sleep(100);
//         printf("\rRecording... %d seconds remaining ", 
//                recording_seconds - (data.frame_index / SAMPLE_RATE));
//         fflush(stdout);
//     }
//     printf("\n");
    
//     // Stop stream
//     err = Pa_StopStream(stream);
//     if (err != paNoError) {
//         fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
//     }
    
//     Pa_CloseStream(stream);
//     Pa_Terminate();
    
//     printf("Recording finished\n");
    
//     // Save audio to temporary file
//     save_audio_to_file(temp_audio_file, &data);
    
//     // Process audio with speech recognition
//     printf("Processing speech...\n");
//     char *recognized_text = recognize_speech(temp_audio_file);
    
//     // Save to text file
//     if (recognized_text) {
//         FILE *fp = fopen(full_path, "w");
//         if (fp) {
//             fprintf(fp, "%s\n", recognized_text);
//             fclose(fp);
//             printf("Voice note saved to %s\n", full_path);
//         } else {
//             fprintf(stderr, "Could not open file %s for writing\n", full_path);
//         }
//         free(recognized_text);
//     } else {
//         fprintf(stderr, "Speech recognition failed\n");
//     }
    
//     // Clean up
//     remove(temp_audio_file);
//     free(data.recorded_samples);
    
//     return 0;
// }
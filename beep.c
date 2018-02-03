#include <soundio/soundio.h>

#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VERSION_STRING "beep"
char *copyright = "Copyright (C) Louis Abraham, 2017."
                  "Use and Distribution subject to GPLv3."
                  "For information: http://www.gnu.org/copyleft/.";

static const float PI = 3.1415926535f;

/* Meaningful Defaults */
#define DEFAULT_FREQ 440.0 /* Middle A */
#define DEFAULT_LENGTH 200 /* milliseconds */
#define DEFAULT_REPS 1
#define DEFAULT_DELAY 100 /* milliseconds */
#define DEFAULT_END_DELAY NO_END_DELAY
#define DEFAULT_STDIN_BEEP NO_STDIN_BEEP

/* Other Constants */
#define NO_END_DELAY 0
#define YES_END_DELAY 1

#define NO_STDIN_BEEP 0
#define LINE_STDIN_BEEP 1
#define CHAR_STDIN_BEEP 2

typedef struct beep_parms_t {
  float freq;     /* tone frequency (Hz)      */
  int length;     /* tone length    (ms)      */
  int reps;       /* # of repetitions         */
  int delay;      /* delay between reps  (ms) */
  int end_delay;  /* do we delay after last rep? */
  int stdin_beep; /* are we using stdin triggers?  We have three options:
                     - just beep and terminate (default)
                     - beep after a line of input
                     - beep after a character of input
                     In the latter two cases, pass the text back out again,
                     so that beep can be tucked appropriately into a text-
                     processing pipe.
                  */
  int verbose;    /* verbose output?          */
  struct beep_parms_t *next; /* in case -n/--new is used. */
} beep_parms_t;

/* print usage and exit */
void usage_bail(const char *executable_name) {
  printf("Usage:\n%s [-f freq] [-l length] [-r reps] [-d delay] "
         "[-D delay] [-s] [-c]\n",
         executable_name);
  printf("%s [Options...] [-n] [--new] [Options...] ... \n", executable_name);
  printf("%s [-h] [--help]\n", executable_name);
  printf("%s [-v] [-V] [--version]\n", executable_name);
  exit(1);
}

void parse_command_line(int argc, char **argv, beep_parms_t *result) {
  int c;

  struct option opt_list[7] = {{"help", 0, NULL, 'h'},
                               {"version", 0, NULL, 'V'},
                               {"new", 0, NULL, 'n'},
                               {"verbose", 0, NULL, 'X'},
                               {"debug", 0, NULL, 'X'},
                               {"device", 1, NULL, 'e'},
                               {0, 0, 0, 0}};
  while ((c = getopt_long(argc, argv, "f:l:r:d:D:schvVne:", opt_list, NULL)) !=
         EOF) {
    int argval = -1; /* handle parsed numbers for various arguments */
    float argfreq = -1;
    switch (c) {
    case 'f': /* freq */
      if (!sscanf(optarg, "%f", &argfreq) || (argfreq >= 20000 /* ack! */) ||
          (argfreq <= 0))
        usage_bail(argv[0]);
      else if (result->freq != 0)
        fprintf(stderr, "WARNING: multiple -f values given, only last "
                        "one is used.\n");
      result->freq = argfreq;
      break;
    case 'l': /* length */
      if (!sscanf(optarg, "%d", &argval) || (argval < 0))
        usage_bail(argv[0]);
      else
        result->length = argval;
      break;
    case 'r': /* repetitions */
      if (!sscanf(optarg, "%d", &argval) || (argval < 0))
        usage_bail(argv[0]);
      else
        result->reps = argval;
      break;
    case 'd': /* delay between reps - WITHOUT delay after last beep*/
      if (!sscanf(optarg, "%d", &argval) || (argval < 0))
        usage_bail(argv[0]);
      else {
        result->delay = argval;
        result->end_delay = NO_END_DELAY;
      }
      break;
    case 'D': /* delay between reps - WITH delay after last beep */
      if (!sscanf(optarg, "%d", &argval) || (argval < 0))
        usage_bail(argv[0]);
      else {
        result->delay = argval;
        result->end_delay = YES_END_DELAY;
      }
      break;
    case 's':
      result->stdin_beep = LINE_STDIN_BEEP;
      break;
    case 'c':
      result->stdin_beep = CHAR_STDIN_BEEP;
      break;
    case 'v':
    case 'V': /* also --version */
      printf("%s\n", VERSION_STRING);
      exit(0);
      break;
    case 'n': /* also --new - create another beep */
      if (result->freq == 0)
        result->freq = DEFAULT_FREQ;
      result->next = (beep_parms_t *)malloc(sizeof(beep_parms_t));
      result->next->freq = 0;
      result->next->length = DEFAULT_LENGTH;
      result->next->reps = DEFAULT_REPS;
      result->next->delay = DEFAULT_DELAY;
      result->next->end_delay = DEFAULT_END_DELAY;
      result->next->stdin_beep = DEFAULT_STDIN_BEEP;
      result->next->verbose = result->verbose;
      result->next->next = NULL;
      result = result->next; /* yes, I meant to do that. */
      break;
    case 'X': /* --debug / --verbose */
      result->verbose = 1;
      break;
    // case 'e': /* also --device */
    //   console_device = strdup(optarg);
    //   break;
    case 'h': /* notice that this is also --help */
    default:
      usage_bail(argv[0]);
    }
  }
  if (result->freq == 0)
    result->freq = DEFAULT_FREQ;
}

static beep_parms_t *parms;
static float ms_offset = 0.0f;
static float rad_offset = 0.0f;

struct SoundIo *soundio = NULL;
static void write_callback(struct SoundIoOutStream *outstream,
                           int frame_count_min, int frame_count_max) {
  const struct SoundIoChannelLayout *layout = &outstream->layout;
  float float_sample_rate = outstream->sample_rate;
  float ms_per_frame = 1000.0f / (float_sample_rate);
  struct SoundIoChannelArea *areas;
  int frames_left = frame_count_max;
  int err;
  float sample = 0.0f;
  float prevpitch = 0;

  while (frames_left > 0) {
    int frame_count = frames_left;

    if ((err =
             soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
      fprintf(stderr, "%s\n", soundio_strerror(err));
      exit(1);
    }

    if (!frame_count)
      break;

    for (int frame = 0; frame < frame_count; frame += 1) {
      ms_offset += ms_per_frame;

      if (parms) {
        float offset =
            parms->length +
            ((parms->end_delay || (parms->reps >= 1)) ? parms->delay : 0);

        if (ms_offset > offset) {

          // signal smoothing
          if (sample < 0.1) {
            // first order approximation of the sinus
            rad_offset = sample;
          } else {
            // continuous extension
            float radians_per_ms = parms->freq * 2.0f * PI / 1000;
            rad_offset = fmod(rad_offset + offset * radians_per_ms, 2.0f * PI);
          }

          ms_offset -= offset;
          parms->reps--;
          if (!parms->reps) {
            if (parms->verbose)
              printf("freq %f length %i delay %i\n", parms->freq, parms->length,
                     parms->delay);
            beep_parms_t *next = parms->next;
            free(parms);
            parms = next;
          }
        }
      }

      if (parms && (ms_offset < parms->length)) {
        float radians_per_ms = parms->freq * 2.0f * PI / 1000;
        sample = sinf(rad_offset + ms_offset * radians_per_ms);
      } else {
        sample *= .95; // exponential decay
      }

      for (int channel = 0; channel < layout->channel_count; channel++) {
        float *ptr =
            (float *)(areas[channel].ptr + areas[channel].step * frame);
        *ptr = sample;
      }
    }

    if ((err = soundio_outstream_end_write(outstream))) {
      fprintf(stderr, "%s\n", soundio_strerror(err));
      exit(1);
    }
    if (!parms) {
      return;
    }

    frames_left -= frame_count;
  }
}

int main(int argc, char **argv) {

  parms = (beep_parms_t *)malloc(sizeof(beep_parms_t));
  parms->freq = 0;
  parms->length = DEFAULT_LENGTH;
  parms->reps = DEFAULT_REPS;
  parms->delay = DEFAULT_DELAY;
  parms->end_delay = DEFAULT_END_DELAY;
  parms->stdin_beep = DEFAULT_STDIN_BEEP;
  parms->verbose = 0;
  parms->next = NULL;

  parse_command_line(argc, argv, parms);
  int verbose = parms->verbose;

  int err;
  soundio = soundio_create();
  if (!soundio) {
    fprintf(stderr, "out of memory\n");
    return 1;
  }

  if ((err = soundio_connect(soundio))) {
    fprintf(stderr, "error connecting: %s", soundio_strerror(err));
    return 1;
  }

  soundio_flush_events(soundio);

  int default_out_device_index = soundio_default_output_device_index(soundio);
  if (default_out_device_index < 0) {
    fprintf(stderr, "no output device found");
    return 1;
  }

  struct SoundIoDevice *device =
      soundio_get_output_device(soundio, default_out_device_index);
  if (!device) {
    fprintf(stderr, "out of memory");
    return 1;
  }
  if (verbose)
    fprintf(stderr, "Output device: %s\n", device->name);

  struct SoundIoOutStream *outstream = soundio_outstream_create(device);
  outstream->format = SoundIoFormatFloat32NE;

  outstream->write_callback = write_callback;

  if ((err = soundio_outstream_open(outstream))) {
    fprintf(stderr, "unable to open device: %s", soundio_strerror(err));
    return 1;
  }
  if (outstream->layout_error)
    fprintf(stderr, "unable to set channel layout: %s\n",
            soundio_strerror(outstream->layout_error));

  if ((err = soundio_outstream_start(outstream))) {
    fprintf(stderr, "unable to start device: %s", soundio_strerror(err));
    return 1;
  }

  double latency = 0;
  soundio_outstream_get_latency(outstream, &latency);
  if (verbose)
    printf("lantency %.02f", latency);
  usleep((int)(1e6 * latency));

  while (parms) {
    soundio_flush_events(soundio);
    sleep(1);
  }

  soundio_outstream_destroy(outstream);
  soundio_device_unref(device);
  soundio_destroy(soundio);
  return 0;
}
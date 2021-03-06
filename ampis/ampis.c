#include "ampis.h"

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void *clk_thread(void *arg)
{
    snd_rawmidi_t** mfd = (snd_rawmidi_t **)arg;
    char midiclk[1] = {0xF8};
    int try = 0;

    while (mode.run) {
    /* Option: Clk Thru */
        if (mode.clock == 1) {
        // Initialisiere Midi Eingang (GPIO)
        // stoppe internes bpm setzen
        // errechne bpm von extern und setze es, wenn es sich ändert
            ;
    /* Option: Clk Generator */
        } else {
            do {
                if (mode.run == 0)
                    goto stopclk;
                try = pthread_mutex_trylock(&mutex);
            } while (try == EBUSY);

            if (snd_rawmidi_write(mfd[1], midiclk, 1) < 0) {
                DEBUG("Problem writing MIDI output\n");
                pthread_mutex_unlock(&mutex);
                break;
            }
            pthread_mutex_unlock(&mutex);

            usleep(sleeptime);
        }
    }
stopclk:
    mode.run = 0;
    return NULL;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/*  this thread will wait until a full recording cycle of arg-many bars is over
    and then switch to play mode */
void *rwdg_thread(void *arg)
{
    /* sleeptime is 1/96th of a bar */
    int t = sleeptime * 96 * *(int *)arg;

    printf("T: %d, ST: %d, A: %d\n", t, sleeptime, *(int *)arg);
    usleep(t);
    mode.rec = 2;
    return NULL;
}

void *play_thread(void *arg)
{

    char buffer[3];
    snd_rawmidi_t** mfd  = (snd_rawmidi_t **)arg;
    int try = 0;
    int readbytes;
    int recstart = 0;

    ampis_recorder_t recorder;
    init_recorder(&recorder);

    while (mode.run) {

    /* Option: recorder */
        if (mode.rec == 1) {
            // INITIALISIERUNG
            struct timespec start;
            recorder.steps = mode.bar;
            clock_gettime(CLOCK_REALTIME, &start);
            recstart = 0;

            // HAUPTSCHLEIFE
            do {
                if ((readbytes = read_midi(mfd[0], buffer)) < 0) {
                    goto stopthru;
                }

                /* got nothing from midi in */
                if (readbytes == 0) {
                    continue;
                }

                /* wait for first key stroke to start recording*/
                if (recstart == 0) {
                    recstart = 1;
                    pthread_t rwdg;
                    if (pthread_create(&rwdg, NULL, rwdg_thread,
                        &recorder.steps) < 0) {
                        DEBUG("Problem starting clk thread\n");
                        goto stopthru;
                    }
                }

                record_link(buffer, &recorder);

                if (snd_rawmidi_write(mfd[1], buffer, readbytes) < 0) {
                    DEBUG("Problem writing MIDI output\n");
                    pthread_mutex_unlock(&mutex);
                    goto stopthru;
                }

                quantize(recorder.actual, &start, recorder.steps);

            } while (mode.rec == 1);

            // end open note
            if ((buffer[0] >> 1) & 0x9) {
                printf("BUFFER : 0x%x%x%x\n", buffer[0], buffer[1], buffer[1]);
                buffer[0] = (buffer[0] & 0x8);
                record_link(buffer, &recorder);
                if (snd_rawmidi_write(mfd[1], buffer, readbytes) < 0) {
                    DEBUG("Problem writing MIDI output\n");
                    pthread_mutex_unlock(&mutex);
                    goto stopthru;
                }
                quantize(recorder.actual, &start, recorder.steps);
            }

            // cleanup recording
            recorder.last = recorder.actual->prev;
            free(recorder.actual);
            recorder.actual = recorder.first;
            DEBUG("Replay\n");
            mode.rec = 3;
            
            // TODO: Liste ausgeben

        // PLAYMODE spiele liste ab
        } else if (mode.rec == 3) {
            /* hard wait untill next bar */

            usleep(play_link(&recorder, buffer));

            if (snd_rawmidi_write(mfd[1], buffer, 3) < 0) {
                DEBUG("Problem writing MIDI output\n");
                pthread_mutex_unlock(&mutex);
                goto stopthru;
            }

    /* Option: midi thru */
        } else if (mode.rec == 0) {
            if ((readbytes = read_midi(mfd[0], buffer)) < 0) {
                goto stopthru;
            }

            /* got nothing from midi in */
            if (readbytes == 0)
                continue;

            /* the slider of music25 is set to 0xb0
             * thus it can only interact with the program
             * not the rocket */
            if ((buffer[0] == 0xb0) && (buffer[1] == 2)) {
                setbpm(((int)buffer[2] * 2) + 46);
                continue; /* dont send data to rocket */
            }

            do {
                if (mode.run == 0)
                    goto stopthru;
                try = pthread_mutex_trylock(&mutex);
            } while (try == EBUSY);

            if (snd_rawmidi_write(mfd[1], buffer, readbytes) < 0) {
                DEBUG("Problem writing MIDI output\n");
                pthread_mutex_unlock(&mutex);
                goto stopthru;
            }
            pthread_mutex_unlock(&mutex);
        }
    }

stopthru:
    mode.run = 0;
    return NULL;
}

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

int main(int argc, char *argv[])
{
    DEBUG("This is ampis\n");
    DEBUG("=============\n\n");

    snd_rawmidi_t* midichan[2];
    pthread_t midiclkthread;
    pthread_t playthread;

    /* ++ INITIALIZATION ++ */
    DEBUG("initializing...\n");

    mode.clock = 0;
    mode.run = 1;
    mode.rec = 0;
    mode.bar = 4;
    setbpm(120);

    if (pthread_mutex_init(&mutex, NULL) != 0){
        DEBUG("Problem initializing mutex\n");
        exit(1);
    }
    if (midi_ports_init(midichan) < 0){
        DEBUG("Problem initializing midi ports\n");
        exit(1);
    }
    if (pthread_create(&midiclkthread, NULL, clk_thread, midichan) < 0) {
        DEBUG("Problem starting clk thread\n");
        exit(1);
    }
    if (pthread_create(&playthread, NULL, play_thread, midichan) < 0) {
        DEBUG("Problem starting play thread\n");
        exit(1);
    }

    /* ++ MAIN LOOP ++ */
    DEBUG("running...\n");
    while (mode.run) {
        if (get_ampis_mode() < 0)
            break;
    };

    /* ++ CLEANUP ++ */
    DEBUG("finalizing...\n");
    mode.run = 0;

    pthread_join(midiclkthread, NULL);
    pthread_join(playthread, NULL);
    pthread_mutex_destroy(&mutex);

    snd_rawmidi_close(midichan[0]);
    snd_rawmidi_close(midichan[1]);
    midichan[0] = NULL;
    midichan[1] = NULL;

    DEBUG("\n=============\n");
    DEBUG("  Good  Bye  \n");

    return 0;
}


#include <iostream>
#include <cstdlib>
#include "RtMidi.h"
#include "mapper/mapper.h"

#define INSTANCES 10

#define NOTE_OFF 0x80
#define NOTE_ON 0x90
#define AFTERTOUCH 0xA0
#define CONTROL_CHANGE 0xB0
#define PROGRAM_CHANGE 0xC0
#define CHANNEL_PRESSURE 0xD0
#define PITCH_WHEEL 0xE0

int done = 0;
mapper_timetag_t tt;

typedef struct _midimap_device {
    char            *name;
    mapper_device   mapper_dev;
    RtMidiIn        *midiin;
    RtMidiOut       *midiout;
    int             is_linked;
    mapper_signal   sig_pitch[16];
    mapper_signal   sig_vel[16];
    mapper_signal   sig_aftrtch[16];
    mapper_signal   sig_ptch_wh[16];
    mapper_signal   sig_poly_pr[16];
    mapper_signal   sig_chan_pr[16];
    mapper_signal   sig_ctrl_ch[16];
    mapper_signal   sig_prog_ch[16];
    struct _midimap_device *next;
} *midimap_device;

struct _midimap_device *inputs = 0;
struct _midimap_device *outputs = 0;

std::vector<unsigned char> outmess (3, 0);

void cleanup_device(midimap_device dev);

int get_channel_from_signame(const char *name)
{
    char channel_str[4] = {0, 0, 0, 0};
    int channel = 0;
    if (name[8] != '.')
        return -1;
    strncpy(channel_str, &name[9], 3);
    channel_str[strchr(channel_str, '/') - channel_str] = 0;
    channel = atoi(channel_str) - 1;
    if (channel < 1 || channel > 16)
        return -1;
    return channel-1;
}

void pitch_handler(mapper_signal sig,
                   mapper_db_signal props,
                   int instance_id,
                   void *value,
                   int count,
                   mapper_timetag_t *timetag)
{
    // noteoff messages passed straight through with no instances
    midimap_device dev = (midimap_device)props->user_data;
    if (!dev)
        return;
    if (!dev->midiout)
        return;

    int channel = get_channel_from_signame(props->name);

    if (value) {
        // make sure pitch instance is matched to velocity and aftertouch instances
        msig_match_instances(sig, dev->sig_vel[channel], instance_id);
        msig_match_instances(sig, dev->sig_aftrtch[channel], instance_id);
    }
}

void velocity_handler(mapper_signal sig,
                      mapper_db_signal props,
                      int instance_id,
                      void *value,
                      int count,
                      mapper_timetag_t *timetag)
{
    // noteon messages passed straight through with no instances
    midimap_device dev = (midimap_device)props->user_data;
    if (!dev)
        return;
    if (!dev->midiout)
        return;

    int channel = get_channel_from_signame(props->name);

    if (value) {
        // make sure velocity instance is matched to pitch and aftertouch instances
        msig_match_instances(sig, dev->sig_pitch[channel], instance_id);
        msig_match_instances(sig, dev->sig_aftrtch[channel], instance_id);
    }

    int *v = (int *)value;

    // output MIDI NOTEON message
    unsigned char note = (long int)msig_instance_value(dev->sig_pitch[channel],
                                                       instance_id, 0);
    outmess[0] = channel + NOTE_ON;
    outmess[1] = note ?: 60;
    outmess[2] = v[0];
    dev->midiout->sendMessage(&outmess);
}

void aftertouch_handler(mapper_signal sig,
                        mapper_db_signal props,
                        int instance_id,
                        void *value,
                        int count,
                        mapper_timetag_t *timetag)
{
    midimap_device dev = (midimap_device)props->user_data;
    if (!dev || !value)
        return;

    int channel = get_channel_from_signame(props->name);

    if (value) {
        // make sure pitch instance is matched to pitch and velocity instances
        msig_match_instances(sig, dev->sig_pitch[channel], instance_id);
        msig_match_instances(sig, dev->sig_vel[channel], instance_id);
    }

    // check if note exists
    if (!msig_instance_value(dev->sig_vel[channel], instance_id, 0))
        return;

    // output MIDI AFTERTOUCH message
    unsigned char note = (long int)msig_instance_value(dev->sig_pitch[channel],
                                                       instance_id, 0);
    int *v = (int *)value;
    outmess[0] = channel + AFTERTOUCH;
    outmess[1] = note ?: 60;
    outmess[2] = v[0];
    dev->midiout->sendMessage(&outmess);
}

void pitch_wheel_handler(mapper_signal sig,
                         mapper_db_signal props,
                         int instance_id,
                         void *value,
                         int count,
                         mapper_timetag_t *timetag)
{
    // pitch wheel messages passed straight through with no instances
    midimap_device dev = (midimap_device)props->user_data;
    if (!dev || !value)
        return;

    int channel = get_channel_from_signame(props->name);
    int *v = (int *)value;

    outmess[0] = channel + PITCH_WHEEL;
    outmess[1] = v[0];
    outmess[2] = v[0] >> 8;
    dev->midiout->sendMessage(&outmess);
}

void control_change_handler(mapper_signal sig,
                            mapper_db_signal props,
                            int instance_id,
                            void *value,
                            int count,
                            mapper_timetag_t *timetag)
{
    // control change messages passed straight through with no instances
    midimap_device dev = (midimap_device)props->user_data;
    if (!dev || !value)
        return;

    int channel = get_channel_from_signame(props->name);
    int *v = (int *)value;

    outmess[0] = channel + CONTROL_CHANGE;
    outmess[1] = v[0];
    outmess[2] = v[1];
    dev->midiout->sendMessage(&outmess);
}

void program_change_handler(mapper_signal sig,
                            mapper_db_signal props,
                            int instance_id,
                            void *value,
                            int count,
                            mapper_timetag_t *timetag)
{
    // program change messages passed straight through with no instances
    midimap_device dev = (midimap_device)props->user_data;
    if (!dev || !value)
        return;

    int channel = get_channel_from_signame(props->name);
    int *v = (int *)value;

    outmess[0] = channel + PROGRAM_CHANGE;
    outmess[1] = v[0];
    dev->midiout->sendMessage(&outmess);
}

void channel_pressure_handler(mapper_signal sig,
                              mapper_db_signal props,
                              int instance_id,
                              void *value,
                              int count,
                              mapper_timetag_t *timetag)
{
    // channel pressure messages passed straight through with no instances
    midimap_device dev = (midimap_device)props->user_data;
    if (!dev || !value)
        return;

    int channel = get_channel_from_signame(props->name);
    int *v = (int *)value;

    outmess[0] = channel + CHANNEL_PRESSURE;
    outmess[1] = v[0];
    dev->midiout->sendMessage(&outmess);
}

void add_input_signals(midimap_device dev)
{
    char signame[64];
    int i, min = 0, max7bit = 127, max14bit = 16383;
    for (i = 0; i < 16; i++) {
        snprintf(signame, 64, "/channel.%i/note/pitch", i+1);
        dev->sig_pitch[i] = mdev_add_input(dev->mapper_dev, signame, 1, 'i', "midinote",
                                           &min, &max7bit, pitch_handler, dev);
        msig_reserve_instances(dev->sig_pitch[i], INSTANCES-1);

        snprintf(signame, 64, "/channel.%i/note/velocity", i+1);
        dev->sig_vel[i] = mdev_add_input(dev->mapper_dev, signame, 1, 'i', 0,
                                         &min, &max7bit, velocity_handler, dev);
        msig_reserve_instances(dev->sig_vel[i], INSTANCES-1);

        snprintf(signame, 64, "/channel.%i/note/aftertouch", i+1);
        dev->sig_aftrtch[i] = mdev_add_input(dev->mapper_dev, signame, 1, 'i', 0,
                                             &min, &max7bit, aftertouch_handler, dev);
        msig_reserve_instances(dev->sig_aftrtch[i], INSTANCES-1);
/*
        snprintf(signame, 64, "/channel.%i/note/pressure", i+1);
        dev->sig_poly_pr[i] = mdev_add_input(dev->mapper_dev, signame, 1, 'i', 0,
                                             &min, &max7bit, poly_pressure_handler, dev);
        msig_reserve_instances(dev->sig_poly_pr[i], INSTANCES-1);
 */
        snprintf(signame, 64, "/channel.%i/pitch_wheel", i+1);
        dev->sig_ptch_wh[i] = mdev_add_input(dev->mapper_dev, signame, 1, 'i', 0,
                                             &min, &max14bit, pitch_wheel_handler, dev);
        msig_reserve_instances(dev->sig_ptch_wh[i], INSTANCES-1);

        // TODO: declare meaningful control change signals
        snprintf(signame, 64, "/channel.%i/control_change", i+1);
        dev->sig_ctrl_ch[i] = mdev_add_input(dev->mapper_dev, signame, 2, 'i', "midi",
                                             &min, &max7bit, control_change_handler, dev);
        msig_reserve_instances(dev->sig_ctrl_ch[i], INSTANCES-1);

        snprintf(signame, 64, "/channel.%i/program_change", i+1);
        dev->sig_prog_ch[i] = mdev_add_input(dev->mapper_dev, signame, 1, 'i', 0,
                                             &min, &max7bit, program_change_handler, dev);
        msig_reserve_instances(dev->sig_prog_ch[i], INSTANCES-1);

        snprintf(signame, 64, "/channel.%i/channel_pressure", i+1);
        dev->sig_chan_pr[i] = mdev_add_input(dev->mapper_dev, signame, 1, 'i', 0,
                                             &min, &max7bit, channel_pressure_handler, dev);
        msig_reserve_instances(dev->sig_chan_pr[i], INSTANCES-1);
    }
}

// Declare output signals
void add_output_signals(midimap_device dev)
{
    char signame[64];
    int i, min = 0, max7bit = 127, max14bit = 16383;
    for (i = 0; i < 16; i++) {
        snprintf(signame, 64, "/channel.%i/note/pitch", i+1);
        dev->sig_pitch[i] = mdev_add_output(dev->mapper_dev, signame, 1,
                                            'i', "midinote", &min, &max7bit);
        msig_reserve_instances(dev->sig_pitch[i], INSTANCES-1);

        snprintf(signame, 64, "/channel.%i/note/velocity", i+1);
        dev->sig_vel[i] = mdev_add_output(dev->mapper_dev, signame, 1,
                                          'i', 0, &min, &max7bit);
        msig_reserve_instances(dev->sig_vel[i], INSTANCES-1);

        snprintf(signame, 64, "/channel.%i/note/aftertouch", i+1);
        dev->sig_aftrtch[i] = mdev_add_output(dev->mapper_dev, signame, 1,
                                              'i', 0, &min, &max7bit);
        msig_reserve_instances(dev->sig_aftrtch[i], INSTANCES-1);
        /*
         snprintf(signame, 64, "/channel.%i/note/pressure", i+1);
         dev->sig_poly_pr[i] = mdev_add_output(dev->mapper_dev, signame, 1,
                                               'i', 0, &min, &max7bit);
         msig_reserve_instances(dev->sig_poly_pr[i], INSTANCES-1);
         */
        snprintf(signame, 64, "/channel.%i/pitch_wheel", i+1);
        dev->sig_ptch_wh[i] = mdev_add_output(dev->mapper_dev, signame, 1,
                                              'i', 0, &min, &max14bit);
        msig_reserve_instances(dev->sig_ptch_wh[i], INSTANCES-1);

        // TODO: declare meaningful control change signals
        snprintf(signame, 64, "/channel.%i/control_change", i+1);
        dev->sig_ctrl_ch[i] = mdev_add_output(dev->mapper_dev, signame, 2,
                                              'i', "midi", &min, &max7bit);
        msig_reserve_instances(dev->sig_ctrl_ch[i], INSTANCES-1);

        snprintf(signame, 64, "/channel.%i/program_change", i+1);
        dev->sig_prog_ch[i] = mdev_add_output(dev->mapper_dev, signame, 1,
                                              'i', 0, &min, &max7bit);
        msig_reserve_instances(dev->sig_prog_ch[i], INSTANCES-1);

        snprintf(signame, 64, "/channel.%i/channel_pressure", i+1);
        dev->sig_chan_pr[i] = mdev_add_output(dev->mapper_dev, signame, 1,
                                              'i', 0, &min, &max7bit);
        msig_reserve_instances(dev->sig_chan_pr[i], INSTANCES-1);
    }
}

void parse_midi(double deltatime, std::vector<unsigned char> *message, void *user_data)
{
    if ((unsigned int)message->size() != 3)
        return;

    midimap_device dev = (midimap_device)user_data;
    if (!mdev_ready(dev->mapper_dev))
        return;

    int msg_type = ((int)message->at(0) - 0x80) / 0x0F;
    int channel = ((int)message->at(0) - 0x80) % 0x0F - 1;
    int data[2] = {(int)message->at(1), (int)message->at(2)};

    mdev_timetag_now(dev->mapper_dev, &tt);
    mdev_start_queue(dev->mapper_dev, tt);
    switch (msg_type) {
        case 0: // note-off message
            msig_release_instance(dev->sig_pitch[channel],
                                  data[0], tt);
            msig_release_instance(dev->sig_vel[channel],
                                  data[0], tt);
            msig_release_instance(dev->sig_aftrtch[channel],
                                  data[0], tt);
            break;
        case 1: // note-on message
            if (data[1]) {
                msig_update_instance(dev->sig_pitch[channel],
                                     data[0], &data[0], 1, tt);
                msig_update_instance(dev->sig_vel[channel],
                                     data[0], &data[1], 1, tt);
            }
            else {
                msig_release_instance(dev->sig_pitch[channel],
                                      data[0], tt);
                msig_release_instance(dev->sig_vel[channel],
                                      data[0], tt);
                msig_release_instance(dev->sig_aftrtch[channel],
                                      data[0], tt);
            }
            break;
        case 2: // aftertouch message
            msig_update_instance(dev->sig_aftrtch[channel],
                                 data[0], &data[1], 1, tt);
            break;
        case 3: // control change message
            msig_update(dev->sig_ctrl_ch[channel], (void *)data, 1, tt);
            break;
        case 4: // program change message
            msig_update(dev->sig_prog_ch[channel], (void *)data, 1, tt);
            break;
        case 5: // channel pressure message
            msig_update(dev->sig_chan_pr[channel], (void *)data, 1, tt);
            break;
        case 6: // pitch wheel message
        {
            int value = data[0] + (data[1] << 8);
            msig_update(dev->sig_ptch_wh[channel], &value, 1, tt);
            break;
        }
        default:
            break;
    }
    mdev_send_queue(dev->mapper_dev, tt);
}

// Check if any MIDI ports are available on the system
void scan_midi_devices()
{
    printf("Searching for MIDI devices...\n");
    char devname[128];

    RtMidiIn *midiin = 0;
    RtMidiOut *midiout = 0;

    try {
        midiin = new RtMidiIn();
        unsigned int nPorts = midiin->getPortCount();
        std::cout << "There are " << nPorts << " MIDI input sources available.\n";

        for (unsigned int i=0; i<nPorts; i++) {
            std::string portName = midiin->getPortName(i);
            std::cout << "  Input Port #" << i+1 << ": " << portName << '\n';
            // check if record already exists
            midimap_device temp = outputs;
            while (temp) {
                if (portName.compare(temp->name) == 0) {
                    break;
                }
                temp = temp->next;
            }
            if (temp)
                continue;
            // new device discovered
            midimap_device dev = (midimap_device) calloc(1, sizeof(struct _midimap_device));
            dev->name = strdup(portName.c_str());
            // remove illegal characters in device name
            unsigned int len = strlen(dev->name), k = 0;
            for (unsigned int j=0; j<len; j++) {
                if (isalnum(dev->name[j])) {
                    devname[k++] = dev->name[j];
                    devname[k] = 0;
                }
            }
            dev->mapper_dev = mdev_new(devname, 0, 0);
            dev->midiin = new RtMidiIn();
            dev->midiin->openPort(i);
            dev->midiin->setCallback(&parse_midi, dev);
            dev->midiin->ignoreTypes(true, true, true);
            dev->next = outputs;
            outputs = dev;
            add_output_signals(dev);
        }
    }
    catch (RtError &error) {
        error.printMessage();
    }

    delete midiin;

    try {
        midiout = new RtMidiOut();
        unsigned int nPorts = midiout->getPortCount();
        std::cout << "There are " << nPorts << " MIDI output ports available.\n";

        for (unsigned int i=0; i<nPorts; i++) {
            std::string portName = midiout->getPortName(i);
            std::cout << "  Output Port #" << i+1 << ": " << portName << '\n';
            // check if record already exists
            midimap_device temp = inputs;
            while (temp) {
                if (portName.compare(temp->name) == 0) {
                    break;
                }
                temp = temp->next;
            }
            if (temp)
                continue;
            // new device discovered
            midimap_device dev = (midimap_device) calloc(1, sizeof(struct _midimap_device));
            dev->name = strdup(portName.c_str());
            // remove illegal characters in device name
            unsigned int len = strlen(dev->name), k = 0;
            for (unsigned int j=0; j<len; j++) {
                if (isalnum(dev->name[j])) {
                    devname[k++] = dev->name[j];
                    devname[k] = 0;
                }
            }
            dev->mapper_dev = mdev_new(devname, 0, 0);
            dev->midiin = 0;
            dev->midiout = new RtMidiOut();
            dev->midiout->openPort(i);
            dev->next = inputs;
            inputs = dev;
            add_input_signals(dev);
        }
    }
    catch (RtError &error) {
        error.printMessage();
    }

    delete midiout;

    return;

    /*
    // check for dropped devices
    midimap_device *temp = &outputs;
    midimap_device found = 0;
    while (*temp) {
        found  = 0;
        for (i = 0; i < Pm_CountDevices(); i++) {
            const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
            if (strcmp((*temp)->name, info->name) == 0) {
                found = *temp;
                break;
            }
        }
        if (!found) {
            // MIDI device has disappeared
            printf("    Removed %s\n", found->name);
            *temp = found->next;
            cleanup_device(found);
        }
        temp = &(*temp)->next;
    }*/
}

void cleanup_device(midimap_device dev)
{
    if (dev->name) {
        free(dev->name);
    }
    if (dev->mapper_dev) {
        mdev_free(dev->mapper_dev);
    }
    if (dev->midiin) {
        delete dev->midiin;
    }
    if (dev->midiout) {
        delete dev->midiout;
    }
}

void cleanup_all_devices()
{
    printf("\nCleaning up!\n");
    midimap_device dev;
    while (inputs) {
        dev = inputs;
        inputs = dev->next;
        cleanup_device(dev);
    }
    while (outputs) {
        dev = outputs;
        outputs = dev->next;
        cleanup_device(dev);
    }
}

void loop()
{
    //int counter = 0;
    midimap_device temp;

    scan_midi_devices();

    while (!done) {
        // poll libmapper outputs
        temp = outputs;
        while (temp) {
            mdev_poll(temp->mapper_dev, 0);
            temp = temp->next;
        }
        // poll libmapper inputs
        temp = inputs;
        while (temp) {
            mdev_poll(temp->mapper_dev, 0);
            temp = temp->next;
        }
        usleep(10 * 1000);
        // TODO: debug & enable MIDI device rescan
        //if (counter++ > 500) {
        //    scan_midi_devices();
        //    counter = 0;
        //}
    }
}

void ctrlc(int sig)
{
    done = 1;
}

int main ()
{
    signal(SIGINT, ctrlc);

    loop();

    cleanup_all_devices();
    return 0;
}
#include <cstdint>
#include <atomic>
#include <algorithm>
#include <cinttypes>

#include "drivers/system.h"
#include "drivers/profiling.h"
#include "drivers/switches.h"
#include "drivers/analog.h"
#include "drivers/sample_memory.h"
#include "drivers/gpio.h"

#include "common/config.h"
#include "common/io.h"
#include "util/buffer_chain.h"
#include "util/edge_detector.h"
#include "monitor/monitor.h"
#include "app/engine/recording_engine.h"
#include "app/engine/playback_engine.h"

// CYCLOPS INCLUDES
#include "app/engine/synth_engine.h"
// CYCLOPS INCLUDES END HERE

namespace recorder
{

    Analog analog_;
    Switches switches_;

    enum State
    {
        STATE_IDLE,
        STATE_SYNTH,
        STATE_RECORD,
        STATE_PLAY,
        STATE_STOP,
        STATE_SAVE,
        STATE_SAVE_ERASE,
        STATE_SAVE_BEGIN_WRITE,
        STATE_SAVE_WRITE,
        STATE_SAVE_COMMIT,
        STATE_STANDBY,
    };

    // CYCLOPS VARIABLES AND SUCH
    SynthEngine synth_engine_;
    // CYCLOPS VARIABLES AND SUCH END HERE

    std::atomic<State> state_;
    uint32_t idle_timeout_;
    uint32_t scrub_idle_timeout_;
    EdgeDetector play_button_;
    EdgeDetector tune_button_;
    uint32_t playback_timeout_;
    float last_pot_value;
    // SampleMemory<__fp16> sample_memory_;
    // RecordingEngine recording_{sample_memory_};
    // PlaybackEngine playback_{sample_memory_};
    DeviceIO io_;
    Monitor monitor_;
    int count = 0;
    OutputPin<GPIOC_BASE, 2> ledPin;

    void Transition(State state)
    {
        printf("State: ");

        switch (state)
        {
        case STATE_IDLE:
            printf("IDLE\n");
            idle_timeout_ = 0;
            break;
        case STATE_RECORD:
            printf("RECORD\n");
            break;
        case STATE_PLAY:
            printf("PLAY\n");
            break;
        case STATE_STOP:
            printf("STOP\n");
            break;
        case STATE_SAVE:
            printf("SAVE\n");
            break;
        case STATE_SAVE_ERASE:
            printf("ERASE\n");
            break;
        case STATE_SAVE_BEGIN_WRITE:
            printf("BEGIN_WRITE\n");
            break;
        case STATE_SAVE_WRITE:
            printf("WRITE\n");
            break;
        case STATE_SAVE_COMMIT:
            printf("COMMIT\n");
            break;
        case STATE_STANDBY:
            printf("STANDBY\n");
            break;
        };

        state_.store(state, std::memory_order_acq_rel);
    }

    void StateMachine(bool standby)
    {
        switches_.Process(io_.human.in);
        play_button_.Process(io_.human.in.sw[SWITCH_PLAY]);
        tune_button_.Process(io_.human.in.sw[SWITCH_TUNE]);
        bool record = io_.human.in.sw[SWITCH_RECORD];
        bool scrub = false; // making this false for current release as it uses record switch so idle scrub isn't necessary

        State state = state_.load(std::memory_order_relaxed);

        if (state == STATE_IDLE)
        {
            /*
            ledPin.Write(0);
            if (record)
            {
                recording_.Reset();
                analog_.StartRecording();
                sample_memory_.StartRecording();
                Transition(STATE_RECORD);
            }
            else if (play_button_.is_high())
            {
                playback_.Reset();
                playback_.Play();
                analog_.StartPlayback();
                sample_memory_.StartPlayback();
                playback_timeout_ = 0;
                Transition(STATE_PLAY);
            }
            */

            if (play_button_.is_high())
            {
                // synth_engine_.Init();
                analog_.Start(true);
                Transition(STATE_SYNTH);
            }
            else if (kEnableIdleStandby &&
                     ++idle_timeout_ > kIdleStandbyTime * 1000)
            {
                printf("Idle timeout expired\n");
                standby = true;
            }

            if (standby)
            {
                Transition(STATE_STANDBY); // Change to state save if we implement recording.
            }
        }
        else if (state == STATE_SYNTH)
        {
            // We will accumulate time (in ms) after the play button is released
            // before transitioning to STATE_IDLE.
            static uint32_t synthReleaseCounter = 0;

            // If the button is now low, increment the release counter:
            if (!synth_engine_.getActive())
            {
                if (++synthReleaseCounter >= 50) // 10 seconds @ ~1ms per loop
                {
                    analog_.Stop();
                    Transition(STATE_IDLE);
                    synthReleaseCounter = 0; // reset the counter
                }
            }
            else
            {
                // If the button is high, reset the counter
                synthReleaseCounter = 0;
            }
        }
        // else if (state == STATE_RECORD)
        // {
        //     ledPin.Write(1);
        //     if (!record)
        //     {
        //         analog_.Stop();
        //         Transition(STATE_IDLE);
        //         sample_memory_.StopRecording();
        //     }
        // }
        // else if (state == STATE_PLAY)
        // {
        //     ledPin.Write(1);
        //     if (analog_.running())
        //     {
        //         if (scrub)
        //         {
        //             if (io_.human.in.pot[POT_4] == last_pot_value)
        //             {
        //                 ++scrub_idle_timeout_;
        //                 if (scrub_idle_timeout_ > 5 * 1000)
        //                 {
        //                     printf("Scrub idle timeout expired\n");
        //                     playback_.StopScrub();
        //                     playback_.Stop();
        //                     Transition(STATE_STANDBY);
        //                 }
        //             }
        //             else
        //             {
        //                 last_pot_value = io_.human.in.pot[POT_4];
        //                 scrub_idle_timeout_ = 0;
        //             }
        //         }
        //         else
        //         {
        //             playback_.StopScrub();
        //         }
        //         if ((++playback_timeout_ == kPlaybackExpireTime * 1000) ||
        //             (play_button_.rising() && playback_.playing()))
        //         {
        //             playback_.Stop();
        //         }
        //         else if (play_button_.rising() && playback_.stopping())
        //         {
        //             playback_.Play();
        //         }
        //         else if (playback_.ended())
        //         {
        //             analog_.Stop();
        //         }
        //     }
        //     else if (analog_.stopped())
        //     {
        //         Transition(STATE_STOP);
        //     }
        // }
        // else if (state == STATE_STOP)
        // {
        //     if (play_button_.is_low())
        //     {
        //         Transition(STATE_IDLE);
        //     }
        // }
        // else if (state == STATE_SAVE)
        // {
        //     if (sample_memory_.dirty())
        //     {
        //         if (sample_memory_.BeginErase())
        //         {
        //             Transition(STATE_SAVE_ERASE);
        //         }
        //         else
        //         {
        //             printf("Erase failed\n");
        //             Transition(STATE_STANDBY);
        //         }
        //     }
        //     else
        //     {
        //         Transition(STATE_STANDBY);
        //     }
        // }
        // else if (state == STATE_SAVE_ERASE)
        // {
        //     if (sample_memory_.FinishErase())
        //     {
        //         Transition(STATE_SAVE_BEGIN_WRITE);
        //     }
        //     else if (record || play_button_.is_high())
        //     {
        //         printf("Save aborted\n");
        //         sample_memory_.AbortErase();
        //         Transition(STATE_IDLE);
        //     }
        // }
        // else if (state == STATE_SAVE_BEGIN_WRITE)
        // {
        //     if (sample_memory_.write_complete())
        //     {
        //         Transition(STATE_SAVE_COMMIT);
        //     }
        //     else if (sample_memory_.BeginWrite())
        //     {
        //         Transition(STATE_SAVE_WRITE);
        //     }
        //     else
        //     {
        //         printf("Write failed\n");
        //         Transition(STATE_STANDBY);
        //     }
        // }
        // else if (state == STATE_SAVE_WRITE)
        // {
        //     if (sample_memory_.FinishWrite())
        //     {
        //         Transition(STATE_SAVE_BEGIN_WRITE);
        //     }
        //     else if (record || play_button_.is_high())
        //     {
        //         printf("Save aborted\n");
        //         sample_memory_.AbortWrite();
        //         Transition(STATE_IDLE);
        //     }
        // }
        // else if (state == STATE_SAVE_COMMIT)
        // {
        //     if (sample_memory_.Commit())
        //     {
        //         printf("Save completed\n");
        //         sample_memory_.PrintInfo("    ");
        //     }
        //     else
        //     {
        //         printf("Commit failed\n");
        //     }

        //     Transition(STATE_STANDBY);
        // }
        else if (state == STATE_STANDBY)
        {
            system::SerialFlushTx();
            analog_.Stop();
            // sample_memory_.PowerDown();
            system::Standby();
        }
    }

    const AudioOutput Process(const AudioInput &audio_in, const PotInput &pot)
    {
        ScopedProfilingPin<PROFILE_PROCESS> profile;
        io_.human.in.pot = pot;
        AudioOutput audio_out = {};
        State state = state_.load(std::memory_order_acquire);

        if (state == STATE_SYNTH)
        {
            bool button_pressed = play_button_.is_high();
            bool tune = tune_button_.is_low();
            bool hold = io_.human.in.sw[SWITCH_LOOP];
            float pot_value = pot[POT_1];
            float vib = pot[POT_3];
            float formant = pot[POT_2];
            synth_engine_.Process(audio_out[AUDIO_OUT_LINE], button_pressed, pot_value, hold, formant, vib, tune);
        }

        return audio_out;
    }

    extern "C" int main(void)
    {

        system::Init();
        ProfilingPin<PROFILE_MAIN>::Set();

        analog_.Init(Process);
        switches_.Init();
        play_button_.Init();
        tune_button_.Init();
        analog_.StartPlayback();
        //   recording_.Init();
        //  playback_.Init();
        synth_engine_.Init();
        io_.Init();
        monitor_.Init();
        system::ReloadWatchdog();
        //  playback_.Reset();
        //  sample_memory_.Init();
        // ledPin.Init(GPIOPin::SPEED_LOW, GPIOPin::TYPE_PUSHPULL, GPIOPin::PULL_NONE);
        Transition(STATE_SYNTH);

        bool expire_watchdog = false;

        if (kADCAlwaysOn)
        {
            analog_.Start(false);
        }

        for (;;)
        {
            printf("test");
            ProfilingPin<PROFILE_MAIN_LOOP>::Set();
            std::atomic_thread_fence(std::memory_order_acq_rel);

            bool standby = false;
            auto message = monitor_.Receive();

            if (message.type == Message::TYPE_QUERY)
            {
                monitor_.Report(io_);
            }
            else if (message.type == Message::TYPE_STANDBY)
            {
                standby = true;
            }
            else if (message.type == Message::TYPE_WATCHDOG)
            {
                expire_watchdog = true;
            }
            else if (message.type == Message::TYPE_RESET)
            {
                system::SerialFlushTx();
                system::Reset();
            }
            else if (message.type == Message::TYPE_ERASE)
            {
                printf("Erasing save data... ");
                // sample_memory_.Erase();
                printf("done\n");
            }

            if (!expire_watchdog)
            {
                system::ReloadWatchdog();
            }

            StateMachine(standby);
            ProfilingPin<PROFILE_MAIN_LOOP>::Clear();

            system::Delay_ms(1);
        }
    }

} // namespace recorder

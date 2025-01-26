#pragma once

#include <cstdint>

#include "common/io.h"
#include "app/monitor/a85.h"
#include "app/monitor/packet.h"
#include "app/monitor/message.h"

namespace recorder
{

class Monitor
{
public:
    void Init(void)
    {
        length_ = 0;
    }

    const Message& Receive(void)
    {
        char* result =
            std::fgets(line_ + length_, sizeof(line_) - length_, stdin);

        if (result != nullptr)
        {
            length_ = std::strlen(line_);
        }

        if (length_ == 0 || line_[length_ - 1] != '\n')
        {
            message_.payload.type = Message::TYPE_NONE;
        }
        else
        {
            if (line_[0] == '\xff' && length_ > 1)
            {
                line_[length_ - 1] = '\0';
                a85::Decode(&message_, sizeof(message_), line_ + 1);

                if (message_.Verify())
                {
                    Ack();
                }
                else
                {
                    message_.payload.type = Message::TYPE_NONE;
                }
            }
            else
            {
                message_.payload.type = Message::TYPE_TEXT;
                line_[length_ - 1] = '\0';
                std::strncpy(message_.payload.text, line_,
                    sizeof(message_.payload.text));
            }

            length_ = 0;
        }

        return message_.payload;
    }

    void Report(const DeviceIO& io)
    {
        PopulateState(io);
        state_.Sign();
        a85::Encode(line_, sizeof(line_), &state_, sizeof(state_));

        printf("\xff%s\n", line_);
    }

protected:
    char line_[sizeof(Message::text)];
    size_t length_;
    Packet<Message> message_;

    struct __attribute__ ((packed)) State
    {
        float pot[NUM_POTS];

        union
        {
            uint8_t bool_bits;
            struct
            {
                bool play : 1;
                bool record : 1;
                bool loop : 1;
                bool reverse : 1;
                bool line_in_detect : 1;
            };
        };
    };

    Packet<State> state_;

    void Ack(void)
    {
        printf("\xff" "ack\n");
    }

    void PopulateState(const DeviceIO& io)
    {
        auto& state = state_.payload;
        auto& human = io.human.in;

        for (uint32_t i = 0; i < NUM_POTS; i++)
        {
            state.pot[i] = human.pot[i];
        }

        state.play = human.sw[SWITCH_PLAY];
        state.record = human.sw[SWITCH_RECORD];
        state.loop = human.sw[SWITCH_LOOP];
        state.reverse = human.sw[SWITCH_REVERSE];
        state.line_in_detect = human.detect[DETECT_LINE_IN];
    }
};

}

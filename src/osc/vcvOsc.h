// copyright (c) openFrameworks team 2010-2017
// copyright (c) Damian Stewart 2007-2009
#pragma once

#include "vcvOscArg.h"
#include "vcvOscMessage.h"
#include "vcvOscSender.hpp"
#include "vcvOscReceiver.h"

struct OscCatOutput : vcvOscSender
{
    float lastValues[128];
    bool lastGates[128];
    std::string host = "127.0.0.1";

    OscCatOutput()
    {
        reset();
    }

    void reset()
    {
        for (int n = 0; n < 128; n++)
        {
            lastValues[n] = -1.0f;
            lastGates[n] = false;
        }
    }

    void sendOscMessage(float value, int cc, bool force = false)
    {
        if (value == lastValues[cc] && !force)
            return;
        lastValues[cc] = value;
        // CC
        vcvOscMessage m;
        m.setAddress("/fader");
        m.addIntArg(cc);
        m.addFloatArg(value);
        sendMessage(m);
    }

    void setGate(float vel, int note, bool noteOffVelocityZero, bool force = false)
    {
        if (vel > 0)
        {
            // Note on
            if (!lastGates[note] || force)
            {
                vcvOscMessage m;
                m.setAddress("/button");
                m.addIntArg(note);
                m.addFloatArg(vel);
                sendMessage(m);
            }
        }
        else if (vel == 0)
        {
            // Note off
            if (lastGates[note] || force)
            {
                vcvOscMessage m;
                m.setAddress("/button");
                m.addIntArg(note);
                m.addFloatArg(0);
                sendMessage(m);
            }
        }
        lastGates[note] = vel > 0;
    }
};

enum class oscMsgType
{
    FADER = 0,
    ENCODER = 1,
    BUTTON = 2
};

class vcvOscAdapter
{
public:
    virtual ~vcvOscAdapter()
    {
        controllerId = -1;
        current = -1.0f;
        lastTs = 0;
    }

    virtual oscMsgType getType() { return type; }
    void setType(oscMsgType type) { this->type = type; }
    float getValue() { return current; }
    virtual bool setValue(float value, uint32_t ts)
    {
        current = value;
        lastTs = ts;
        return true;
    }
    void reset()
    {
        current = -1.0f;
        controllerId = -1;
    }
    void resetValue() { current = -1.0f; }
    virtual void sendFeedback(float value, bool sendOnly) {}
    int getControllerId() { return controllerId; }
    void setControllerId(int controllerId) { this->controllerId = controllerId; }
    void setTs(uint32_t ts) { this->lastTs = ts; }
    uint32_t getTs() { return lastTs; }

private:
    int controllerId = -1;
    uint32_t lastTs = 0;
    oscMsgType type;
    float current;
};

class vcvOscFader : public vcvOscAdapter
{
public:
    vcvOscFader(int controllerId)
    {
        this->setControllerId(controllerId);
        this->setType(oscMsgType::FADER);
        vcvOscAdapter::setValue(-1.0f, 0);
    }

    bool setValue(float value, uint32_t ts) override
    {
        float previous = this->getValue();
        if (ts > this->getTs())
        {
            vcvOscAdapter::setValue(value, ts);
        }

        return this->getValue() >= 0.f && this->getValue() != previous;
    }
};

class vcvOscEncoder : public vcvOscAdapter
{
public:
    vcvOscEncoder(int controllerId, float value, uint32_t ts, int steps = 649)
    {
        this->setControllerId(controllerId);
        this->setType(oscMsgType::ENCODER);
        this->setValue(value, ts);
        this->setSteps(steps);
    }

    bool setValue(float value, uint32_t ts) override
    {
        float previous = vcvOscAdapter::getValue();
        if (ts > vcvOscAdapter::getTs())
        {
            if (previous < 0.0f)
                vcvOscAdapter::setValue(0.f + (value / steps), ts);
            else
                vcvOscAdapter::setValue(previous + (value / steps), ts);
        }
        return vcvOscAdapter::getValue() >= 0.f && vcvOscAdapter::getValue() != previous;
    }

    void setSteps(int steps)
    {
        this->steps = steps;
    }

private:
    int steps = 649;
};
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

class vcvOscController
{
public:
    static vcvOscController *Create(std::string address, int controllerId, float value = -1.f, uint32_t ts = 0);

    virtual ~vcvOscController()
    {
        controllerId = -1;
        current = -1.0f;
        lastTs = 0;
    }

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
    void setAddress(std::string address) { this->address = address; }
    std::string getAddress() { return address; }

private:
    int controllerId = -1;
    uint32_t lastTs = 0;
    float current;
    std::string address;
};

class vcvOscFader : public vcvOscController
{
public:
    vcvOscFader(int controllerId, float value = -1.f, uint32_t ts = 0)
    {
        this->setAddress("/fader");
        this->setControllerId(controllerId);
        vcvOscController::setValue(value, ts);
    }

    virtual bool setValue(float value, uint32_t ts) override
    {
        float previous = this->getValue();
        if (ts > this->getTs())
        {
            vcvOscController::setValue(value, ts);
        }

        return this->getValue() >= 0.f && this->getValue() != previous;
    }
};

class vcvOscEncoder : public vcvOscController
{
public:
    vcvOscEncoder(int controllerId, float value, uint32_t ts, int steps = 649)
    {
        this->setAddress("/encoder");
        this->setControllerId(controllerId);
        this->setValue(value, ts);
        this->setSteps(steps);
    }

    virtual bool setValue(float value, uint32_t ts) override
    {
        float previous = this->getValue();
        float newValue;
        if (ts > this->getTs())
        {
            if (previous < 0.0f)
            {
                newValue = 0.f + (value / float(steps));
                vcvOscController::setValue(clamp(newValue, 0.f, 1.f), ts);
            }
            else
            {
                newValue = previous + (value / float(steps));
                vcvOscController::setValue(clamp(newValue, 0.f, 1.f), ts);
            }
        }
        return this->getValue() >= 0.f && this->getValue() != previous;
    }

    void setSteps(int steps)
    {
        this->steps = steps;
    }

private:
    int steps = 649;
};

vcvOscController *vcvOscController::Create(std::string address, int controllerId, float value, uint32_t ts)
{
    if (address == "/fader")
    {
        return new vcvOscFader(controllerId, value, ts);
    }
    else if (address == "/encoder")
    {
        return new vcvOscEncoder(controllerId, value, ts);
    }
    else
        return NULL;
};
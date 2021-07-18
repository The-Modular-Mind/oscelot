#include "plugin.hpp"
// #pragma once

#include "vcvOscArg.h"
#include "vcvOscMessage.h"
#include "vcvOscSender.hpp"
#include "vcvOscReceiver.h"

// enum class CCMODE {
// 	DIRECT = 0,
// 	PICKUP1 = 1,
// 	PICKUP2 = 2,
// 	TOGGLE = 3,
// 	TOGGLE_VALUE = 4
// };

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
    void setCCMode(TheModularMind::Oscelot::CCMODE CCMode) { this->CCMode = CCMode; }
    TheModularMind::Oscelot::CCMODE getCCMode() { return CCMode; }

private:
    int controllerId = -1;
    uint32_t lastTs = 0;
    float current;
    std::string address;
    TheModularMind::Oscelot::CCMODE CCMode;
};

class vcvOscFader : public vcvOscController
{
public:
    vcvOscFader(int controllerId, float value, uint32_t ts)
    {
        this->setAddress("/fader");
        this->setControllerId(controllerId);
        vcvOscController::setValue(value, ts);
    }

    virtual bool setValue(float value, uint32_t ts) override
    {
        if (ts == 0 || ts > this->getTs())
        {
            vcvOscController::setValue(value, ts);
        }

        return this->getValue() >= 0.f;
    }
};

class vcvOscEncoder : public vcvOscController
{
public:
    vcvOscEncoder(int controllerId, float value, uint32_t ts, int steps = 649)
    {
        this->setAddress("/encoder");
        this->setControllerId(controllerId);
        this->setSteps(steps);
        this->setValue(value, ts);
    }

    virtual bool setValue(float value, uint32_t ts) override
    {
        if (ts == 0) {
			vcvOscController::setValue(value, ts);
		}
        else if (ts > this->getTs()) {
			float newValue = this->getValue() + (value / float(steps));
			vcvOscController::setValue(clamp(newValue, 0.f, 1.f), ts);
		}
        return this->getValue() >= 0.f;
    }

    void setSteps(int steps)
    {
        this->steps = steps;
    }

private:
    int steps = 649;
};

class vcvOscButton : public vcvOscController
{
public:
    vcvOscButton(int controllerId, float value, uint32_t ts)
    {
        this->setAddress("/button");
        this->setControllerId(controllerId);
        vcvOscController::setValue(value, ts);
    }

    virtual bool setValue(float value, uint32_t ts) override
    {
		INFO("Button.setValue(%f, %i, %f)", value, ts);
		if (ts == 0)
        {
            vcvOscController::setValue(value, ts);
        }
        else if (ts > this->getTs())
        {
            vcvOscController::setValue(clamp(value, 0.f, 1.0f), ts);
        }
		INFO("Button #%i set %i", this->getControllerId(), this->getValue() >= 0.f);
        return this->getValue() >= 0.f;
    }
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
    else if (address == "/button")
    {
        return new vcvOscButton(controllerId, value, ts);
    }
    else
        INFO("Not Implemented for address: %s", address.c_str());
        return NULL;
};
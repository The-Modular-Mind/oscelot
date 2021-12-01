#pragma once
#include "oscpack/osc/OscTypes.h"

namespace TheModularMind {

class OscArg {
   public:
	virtual ~OscArg() {}
	virtual osc::TypeTagValues getType() const { return osc::NIL_TYPE_TAG; }
};

class OscArgInt32 : public OscArg {
   public:
	OscArgInt32(std::int32_t value) : value(value) {}
	osc::TypeTagValues getType() const override { return osc::INT32_TYPE_TAG; }
	std::int32_t get() const { return value; }
	void set(std::int32_t value) { this->value = value; }

   private:
	std::int32_t value;
};

class OscArgFloat : public OscArg {
   public:
	OscArgFloat(float value) : value(value) {}
	osc::TypeTagValues getType() const override { return osc::FLOAT_TYPE_TAG; }
	float get() const { return value; }
	void set(float value) { this->value = value; }

   private:
	float value;
};

class OscArgString : public OscArg {
   public:
	OscArgString(const std::string &value) : value(value) {}
	osc::TypeTagValues getType() const override { return osc::STRING_TYPE_TAG; }
	const std::string &get() const { return value; }
	void set(const std::string &value) { this->value = value; }
	void set(const char *value) { this->value = value; }

   private:
	std::string value;
};
}  // namespace TheModularMind
#pragma once
#include "../plugin.hpp"
#include "../../../oscpack/osc/OscTypes.h"

/// OSC argument type enum values
///
// enum TypeTagValues
// {
//     TRUE_TYPE_TAG = 'T',
//     FALSE_TYPE_TAG = 'F',
//     NIL_TYPE_TAG = 'N',
//     INFINITUM_TYPE_TAG = 'I',
//     INT32_TYPE_TAG = 'i',
//     FLOAT_TYPE_TAG = 'f',
//     CHAR_TYPE_TAG = 'c',
//     RGBA_COLOR_TYPE_TAG = 'r',
//     MIDI_MESSAGE_TYPE_TAG = 'm',
//     INT64_TYPE_TAG = 'h',
//     TIME_TAG_TYPE_TAG = 't',
//     DOUBLE_TYPE_TAG = 'd',
//     STRING_TYPE_TAG = 's',
//     SYMBOL_TYPE_TAG = 'S',
//     BLOB_TYPE_TAG = 'b',
//     ARRAY_BEGIN_TYPE_TAG = '[',
//     ARRAY_END_TYPE_TAG = ']'
// };

/// \class vcvOscArg
/// \brief base class for arguments
class vcvOscArg{
public:
	virtual ~vcvOscArg() {}
	virtual osc::TypeTagValues getType() const {return osc::NIL_TYPE_TAG;}
};

/// \class vcvOscArgInt32
/// \brief a 32-bit integer argument, type name "i"
class vcvOscArgInt32 : public vcvOscArg{
public:
	vcvOscArgInt32(std::int32_t value) : value(value) {}
	osc::TypeTagValues getType() const override {return osc::INT32_TYPE_TAG;}
	std::int32_t get() const {return value;}
	void set(std::int32_t value) {this->value = value;}

private:
	std::int32_t value;
};

/// \class vcvOscArgFloat
/// \brief a 32-bit float argument, type name "f"
class vcvOscArgFloat : public vcvOscArg{
public:
	vcvOscArgFloat(float value) : value(value) {}
	osc::TypeTagValues getType() const override {return osc::FLOAT_TYPE_TAG;}
	float get() const {return value;}
	void set(float value) {this->value = value;}

private:
	float value;
};

/// \class vcvOscArgString
/// \brief a null-terminated string argument, type name "s"
class vcvOscArgString : public vcvOscArg{
public:
	vcvOscArgString(const std::string &value ) : value(value) {}
	osc::TypeTagValues getType() const override {return osc::STRING_TYPE_TAG;}
	const std::string &get() const {return value;}
	void set(const std::string &value) {this->value = value;}

	/// set value using C string
	void set(const char *value) {this->value = value;}

private:
	std::string value;
};


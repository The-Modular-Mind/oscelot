#pragma once
#include "../osc/OscController.hpp"

namespace TheModularMind {

struct ModuleMeowMoryParam {
	int paramId = -1;
	std::string address;
	int controllerId = -1;
	int encSensitivity = OscController::ENCODER_DEFAULT_SENSITIVITY;
	CONTROLLERMODE controllerMode;
	std::string label = "";

	void fromMappings(ParamHandle paramHandle, OscController* oscController, std::string textLabel) {
		if (paramHandle.moduleId != -1) paramId = paramHandle.paramId;
		label = textLabel;

		if (oscController) {
			controllerId = oscController->getControllerId();
			address = oscController->getAddress();
			controllerMode = oscController->getControllerMode();
			if (oscController->getSensitivity() != OscController::ENCODER_DEFAULT_SENSITIVITY) encSensitivity = oscController->getSensitivity();
		}
	}

	void fromJson(json_t* meowMoryParamJ) {
		paramId = json_integer_value(json_object_get(meowMoryParamJ, "paramId"));

		json_t* labelJ = json_object_get(meowMoryParamJ, "label");
		if (labelJ) label = json_string_value(labelJ);

		json_t* controllerIdJ = json_object_get(meowMoryParamJ, "controllerId");
		if (controllerIdJ) {
			address = json_string_value(json_object_get(meowMoryParamJ, "address"));
			controllerMode = (CONTROLLERMODE)json_integer_value(json_object_get(meowMoryParamJ, "controllerMode"));
			controllerId = json_integer_value(controllerIdJ);

			json_t* encSensitivityJ = json_object_get(meowMoryParamJ, "encSensitivity");
			if (encSensitivityJ) encSensitivity = json_integer_value(encSensitivityJ);
		}
	}

	json_t* toJson() {
		json_t* meowMoryParamJ = json_object();
		if (paramId != -1) {
			json_object_set_new(meowMoryParamJ, "paramId", json_integer(paramId));
		}
		if (controllerId != -1) {
			json_object_set_new(meowMoryParamJ, "controllerId", json_integer(controllerId));
			json_object_set_new(meowMoryParamJ, "controllerMode", json_integer((int)controllerMode));
			json_object_set_new(meowMoryParamJ, "address", json_string(address.c_str()));
			if (encSensitivity != OscController::ENCODER_DEFAULT_SENSITIVITY) json_object_set_new(meowMoryParamJ, "encSensitivity", json_integer(encSensitivity));
		}
		if (label != "") json_object_set_new(meowMoryParamJ, "label", json_string(label.c_str()));

		return meowMoryParamJ;
	}
};

struct BankMeowMoryParam : ModuleMeowMoryParam {
	int64_t moduleId = -1;

	void fromMappings(ParamHandle paramHandle, OscController* oscController, std::string textLabel) {
		ModuleMeowMoryParam::fromMappings(paramHandle, oscController, textLabel);
		if (paramHandle.moduleId != -1) moduleId = paramHandle.moduleId;
	}

	void fromJson(json_t* bankMeowMoryParamJ) {
		ModuleMeowMoryParam::fromJson(bankMeowMoryParamJ);
		moduleId = json_integer_value(json_object_get(bankMeowMoryParamJ, "moduleId"));
	}

	json_t* toJson() {
		json_t* bankMeowMoryParamJ = json_object();
		bankMeowMoryParamJ = ModuleMeowMoryParam::toJson();
		if (moduleId > 0) json_object_set_new(bankMeowMoryParamJ, "moduleId", json_integer(moduleId));
		return bankMeowMoryParamJ;
	}
};

struct ModuleMeowMory {
	std::string pluginName;
	std::string moduleName;
	std::list<ModuleMeowMoryParam> paramArray;

	~ModuleMeowMory() { paramArray.clear(); }

	json_t* toJson() {
		json_t* meowMoryJ = json_object();
		json_object_set_new(meowMoryJ, "pluginName", json_string(pluginName.c_str()));
		json_object_set_new(meowMoryJ, "moduleName", json_string(moduleName.c_str()));
		json_t* paramArrayJ = json_array();
		for (auto& param : paramArray) {
			json_array_append_new(paramArrayJ, param.toJson());
		}
		json_object_set_new(meowMoryJ, "params", paramArrayJ);
		return meowMoryJ;
	}

	void fromJson(json_t* meowMoryJ) {
		pluginName = json_string_value(json_object_get(meowMoryJ, "pluginName"));
		moduleName = json_string_value(json_object_get(meowMoryJ, "moduleName"));
		json_t* paramArrayJ = json_object_get(meowMoryJ, "params");
		size_t j;
		json_t* meowMoryParamJ;
		json_array_foreach(paramArrayJ, j, meowMoryParamJ) {
			ModuleMeowMoryParam meowMoryParam = ModuleMeowMoryParam();
			meowMoryParam.fromJson(meowMoryParamJ);
			paramArray.push_back(meowMoryParam);
		}
	}
};

struct BankMeowMory {
	std::list<BankMeowMoryParam> bankParamArray;

	~BankMeowMory() { bankParamArray.clear(); }

	json_t* toJson() {
		json_t* bankMeowMoryJ = json_object();
		json_t* bankParamJ = json_object();
		json_t* bankParamArrayJ = json_array();
		for (auto& bankParam : bankParamArray) {
			bankParamJ = bankParam.toJson();
			if (json_object_size(bankParamJ) > 0) json_array_append_new(bankParamArrayJ, bankParamJ);
		}
		if (json_array_size(bankParamArrayJ) > 0) {
			json_object_set_new(bankMeowMoryJ, "params", bankParamArrayJ);
		}
		return bankMeowMoryJ;
	}

	void fromJson(json_t* bankMeowMoryJ) {
		json_t* bankParamArrayJ = json_object_get(bankMeowMoryJ, "params");
		size_t j;
		json_t* bankMeowMoryParamJ;
		json_array_foreach(bankParamArrayJ, j, bankMeowMoryParamJ) {
			BankMeowMoryParam bankMeowMoryParam;
			bankMeowMoryParam.fromJson(bankMeowMoryParamJ);
			bankParamArray.push_back(bankMeowMoryParam);
		}
	}
};

}  // namespace TheModularMind
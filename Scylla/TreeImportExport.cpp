#include "TreeImportExport.h"
#include "Architecture.h"
#include "Scylla.h"
#include "StringConversion.h"

TreeImportExport::TreeImportExport(const WCHAR* targetXmlFile)
{
	wcscpy_s(xmlPath, targetXmlFile);
}

bool TreeImportExport::exportTreeList(const std::map<DWORD_PTR, ImportModuleThunk>& moduleList, const Process* process, DWORD_PTR addressOEP, DWORD_PTR addressIAT, DWORD sizeIAT)
{
	tinyxml2::XMLDocument doc;

	tinyxml2::XMLDeclaration* decl = doc.NewDeclaration();
	doc.LinkEndChild(decl);

	tinyxml2::XMLElement* rootElement = doc.NewElement("target");

	setTargetInformation(rootElement, process, addressOEP, addressIAT, sizeIAT);

	addModuleListToRootElement(doc, rootElement, moduleList);

	doc.LinkEndChild(rootElement);

	return saveXmlToFile(doc, xmlPath);
}

bool TreeImportExport::importTreeList(std::map<DWORD_PTR, ImportModuleThunk>& moduleList, DWORD_PTR* addressOEP, DWORD_PTR* addressIAT, DWORD* sizeIAT)
{
	moduleList.clear();
	*addressOEP = *addressIAT = 0;
	*sizeIAT = 0;

	tinyxml2::XMLDocument doc;
	if (!readXmlFile(doc, xmlPath))
	{
		Scylla::windowLog.log(L"Load Tree :: Error parsing xml %S: %S\r\n", doc.Value(), doc.ErrorStr());
		return false;
	}

	tinyxml2::XMLElement* targetElement = doc.FirstChildElement();
	if (!targetElement)
	{
		Scylla::windowLog.log(L"Load Tree :: Error getting first child element in xml %S\r\n", doc.Value());
		return false;
	}

	*addressOEP = ConvertStringToDwordPtr(targetElement->Attribute("oep_va"));
	*addressIAT = ConvertStringToDwordPtr(targetElement->Attribute("iat_va"));
	*sizeIAT = (DWORD)ConvertStringToDwordPtr(targetElement->Attribute("iat_size"));

	parseAllElementModules(targetElement, moduleList);

	return true;
}

void TreeImportExport::setTargetInformation(tinyxml2::XMLElement* rootElement, const Process* process, DWORD_PTR addressOEP, DWORD_PTR addressIAT, DWORD sizeIAT)
{
	StringConversion::ToASCII(process->filename, xmlStringBuffer, _countof(xmlStringBuffer));
	rootElement->SetAttribute("filename", xmlStringBuffer);

	ConvertDwordPtrToString(addressOEP);
	rootElement->SetAttribute("oep_va", xmlStringBuffer);

	ConvertDwordPtrToString(addressIAT);
	rootElement->SetAttribute("iat_va", xmlStringBuffer);

	ConvertDwordPtrToString(sizeIAT);
	rootElement->SetAttribute("iat_size", xmlStringBuffer);
}

bool TreeImportExport::readXmlFile(tinyxml2::XMLDocument& doc, const WCHAR* xmlFilePath)
{
	bool success = false;

	FILE* pFile = 0;
	if (_wfopen_s(&pFile, xmlFilePath, L"rb") == 0)
	{
		success = doc.LoadFile(pFile);
		fclose(pFile);
	}

	return success;
}

bool TreeImportExport::saveXmlToFile(const tinyxml2::XMLDocument& doc, const WCHAR* xmlFilePath)
{
	FILE* pFile = 0;
	if (_wfopen_s(&pFile, xmlFilePath, L"wb") == 0)
	{
		tinyxml2::XMLPrinter printer(pFile);
		doc.Print(&printer);
		fclose(pFile);
		return true;
	}
	else
	{
		return false;
	}
}

void TreeImportExport::addModuleListToRootElement(tinyxml2::XMLDocument& doc, tinyxml2::XMLElement* rootElement, const std::map<DWORD_PTR, ImportModuleThunk>& moduleList)
{
	std::map<DWORD_PTR, ImportModuleThunk>::const_iterator it_mod;
	for (it_mod = moduleList.begin(); it_mod != moduleList.end(); it_mod++)
	{
		const ImportModuleThunk& importModuleThunk = it_mod->second;

		tinyxml2::XMLElement* moduleElement = getModuleXmlElement(doc, &importModuleThunk);

		std::map<DWORD_PTR, ImportThunk>::const_iterator it_thunk;
		for (it_thunk = importModuleThunk.thunkList.begin(); it_thunk != importModuleThunk.thunkList.end(); it_thunk++)
		{
			const ImportThunk& importThunk = it_thunk->second;

			tinyxml2::XMLElement* importElement = getImportXmlElement(doc, &importThunk);

			moduleElement->LinkEndChild(importElement);
		}

		rootElement->LinkEndChild(moduleElement);
	}
}

tinyxml2::XMLElement* TreeImportExport::getModuleXmlElement(tinyxml2::XMLDocument& doc, const ImportModuleThunk* importModuleThunk)
{
	tinyxml2::XMLElement* moduleElement = doc.NewElement("module");

	StringConversion::ToASCII(importModuleThunk->moduleName, xmlStringBuffer, _countof(xmlStringBuffer));
	moduleElement->SetAttribute("filename", xmlStringBuffer);

	ConvertDwordPtrToString(importModuleThunk->getFirstThunk());
	moduleElement->SetAttribute("first_thunk_rva", xmlStringBuffer);

	return moduleElement;
}

tinyxml2::XMLElement* TreeImportExport::getImportXmlElement(tinyxml2::XMLDocument& doc, const ImportThunk* importThunk)
{
	tinyxml2::XMLElement* importElement = nullptr;

	if (importThunk->valid)
	{
		importElement = doc.NewElement("import_valid");

		if (importThunk->name[0] != '\0')
		{
			importElement->SetAttribute("name", importThunk->name);
		}

		ConvertWordToString(importThunk->ordinal);
		importElement->SetAttribute("ordinal", xmlStringBuffer);

		ConvertWordToString(importThunk->hint);
		importElement->SetAttribute("hint", xmlStringBuffer);

		ConvertBoolToString(importThunk->suspect);
		importElement->SetAttribute("suspect", xmlStringBuffer);
	}
	else
	{
		importElement = doc.NewElement("import_invalid");
	}

	ConvertDwordPtrToString(importThunk->rva);
	importElement->SetAttribute("iat_rva", xmlStringBuffer);

	ConvertDwordPtrToString(importThunk->apiAddressVA);
	importElement->SetAttribute("address_va", xmlStringBuffer);

	return importElement;
}

void TreeImportExport::ConvertBoolToString(const bool boolValue)
{
	if (boolValue)
	{
		strcpy_s(xmlStringBuffer, "1");
	}
	else
	{
		strcpy_s(xmlStringBuffer, "0");
	}
}

bool TreeImportExport::ConvertStringToBool(const char* strValue)
{
	if (strValue)
	{
		if (strValue[0] == '1')
		{
			return true;
		}
	}

	return false;
}

void TreeImportExport::ConvertDwordPtrToString(const DWORD_PTR dwValue)
{
	sprintf_s(xmlStringBuffer, PRINTF_DWORD_PTR_FULL_S, dwValue);
}

DWORD_PTR TreeImportExport::ConvertStringToDwordPtr(const char* strValue)
{
	DWORD_PTR result = 0;

	if (strValue)
	{
#ifdef _WIN64
		result = _strtoi64(strValue, NULL, 16);
#else
		result = strtoul(strValue, NULL, 16);
#endif
	}

	return result;
}

void TreeImportExport::ConvertWordToString(const WORD dwValue)
{
	sprintf_s(xmlStringBuffer, "%04X", dwValue);
}

WORD TreeImportExport::ConvertStringToWord(const char* strValue)
{
	WORD result = 0;

	if (strValue)
	{
		result = (WORD)strtoul(strValue, NULL, 16);
	}

	return result;
}

void TreeImportExport::parseAllElementModules(tinyxml2::XMLElement* targetElement, std::map<DWORD_PTR, ImportModuleThunk>& moduleList)
{
	ImportModuleThunk importModuleThunk;

	for (tinyxml2::XMLElement* moduleElement = targetElement->FirstChildElement(); moduleElement; moduleElement = moduleElement->NextSiblingElement())
	{
		const char* filename = moduleElement->Attribute("filename");
		if (filename)
		{
			StringConversion::ToUTF16(filename, importModuleThunk.moduleName, _countof(importModuleThunk.moduleName));

			importModuleThunk.firstThunk = ConvertStringToDwordPtr(moduleElement->Attribute("first_thunk_rva"));

			importModuleThunk.thunkList.clear();
			parseAllElementImports(moduleElement, &importModuleThunk);

			moduleList[importModuleThunk.firstThunk] = importModuleThunk;
		}
	}
}

void TreeImportExport::parseAllElementImports(tinyxml2::XMLElement* moduleElement, ImportModuleThunk* importModuleThunk)
{
	ImportThunk importThunk;

	for (tinyxml2::XMLElement* importElement = moduleElement->FirstChildElement(); importElement; importElement = importElement->NextSiblingElement())
	{
		const char* temp = importElement->Value();

		if (!strcmp(temp, "import_valid"))
		{
			temp = importElement->Attribute("name");
			if (temp)
			{
				strcpy_s(importThunk.name, temp);
			}
			else
			{
				importThunk.name[0] = 0;
			}

			wcscpy_s(importThunk.moduleName, importModuleThunk->moduleName);

			importThunk.suspect = ConvertStringToBool(importElement->Attribute("suspect"));
			importThunk.ordinal = ConvertStringToWord(importElement->Attribute("ordinal"));
			importThunk.hint = ConvertStringToWord(importElement->Attribute("hint"));

			importThunk.valid = true;
		}
		else
		{
			importThunk.valid = false;
			importThunk.suspect = true;
		}

		importThunk.apiAddressVA = ConvertStringToDwordPtr(importElement->Attribute("address_va"));
		importThunk.rva = ConvertStringToDwordPtr(importElement->Attribute("iat_rva"));

		if (importThunk.rva != 0)
		{
			importModuleThunk->thunkList[importThunk.rva] = importThunk;
		}

	}
}

// 
//  Copyright 2012 Andrew Okin
// 
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
// 
//        http://www.apache.org/licenses/LICENSE-2.0
// 
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

#include "instance.h"

const wxString cfgFileName = _("instance.cfg");

bool IsValidInstance(wxFileName rootDir)
{
	return rootDir.DirExists() && Path::Combine(rootDir, cfgFileName).FileExists();
}

Instance *Instance::LoadInstance(wxFileName rootDir)
{
	Instance *inst = new Instance(rootDir, _(""));
	if (!inst->Load())
		return (Instance*)(NULL);
	return inst;
}

Instance::Instance(wxFileName rootDir, wxString name)
{
	this->rootDir = rootDir;
	Load(true);
	this->name = name;
}

Instance::~Instance(void)
{

}

bool Instance::Save() const
{
	if (!GetRootDir().DirExists())
	{
		GetRootDir().Mkdir();
	}

	wxFileName filename = GetConfigPath();
	using boost::property_tree::ptree;
	ptree pt;

	pt.put<std::string>("name", stdStr(name));
	pt.put<std::string>("iconKey", stdStr(iconKey));
	pt.put<std::string>("notes", stdStr(notes));
	pt.put<bool>("NeedsRebuild", needsRebuild);
	pt.put<bool>("AskUpdate", askUpdate);

	write_ini(stdStr(filename.GetFullPath()).c_str(), pt);
	return true;
}

bool Instance::Load(bool loadDefaults)
{
	using boost::property_tree::ptree;
	ptree pt;

	wxFileName filename = GetConfigPath();
	try
	{
		if (!loadDefaults)
			read_ini(stdStr(filename.GetFullPath()).c_str(), pt);
	}
	catch (boost::property_tree::ini_parser_error e)
	{
		wxLogError(_("Failed to parse instance config file '%s'. %s"), 
			stdStr(filename.GetFullPath()).c_str(),
			e.message().c_str());
		return false;
	}

	name = wxStr(pt.get<std::string>("name", "Unnamed Instance"));
	iconKey = wxStr(pt.get<std::string>("iconKey", "default"));
	notes = wxStr(pt.get<std::string>("notes", ""));
	
	needsRebuild = pt.get<bool>("NeedsRebuild", false);
	askUpdate = pt.get<bool>("AskUpdate", true);
	return true;
}

wxFileName Instance::GetRootDir() const
{
	return rootDir;
}

wxFileName Instance::GetConfigPath() const
{
	return wxFileName(rootDir.GetFullPath(), cfgFileName);
}

wxString Instance::GetName() const
{
	return name;
}

void Instance::SetName(wxString name)
{
	this->name = name;
}

wxString Instance::GetIconKey() const
{
	return iconKey;
}

void Instance::SetIconKey(wxString iconKey)
{
	this->iconKey = iconKey;
}
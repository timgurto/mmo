#pragma once

#include <string>

class Client;
class XmlReader;

class CDataLoader {
 public:
  using Directory = std::string;
  using XML = std::string;
  static CDataLoader FromPath(Client &client, const Directory &path = "Data");
  static CDataLoader FromString(Client &client, const XML &data);

  void load(bool keepOldData = false);

  void loadTerrain(XmlReader &reader);
  void loadParticles(XmlReader &reader);
  void loadSounds(XmlReader &reader);
  void loadProjectiles(XmlReader &reader);
  void loadSpells(XmlReader &reader);
  void loadBuffs(XmlReader &reader);
  void loadObjectTypes(XmlReader &reader);
  void loadItems(XmlReader &reader);
  void loadClasses(XmlReader &reader);
  void loadRecipes(XmlReader &reader);
  void loadNPCTypes(XmlReader &reader);
  void loadMap(XmlReader &reader);

 private:
  CDataLoader(Client &client);

  Client &_client;

  // Only one of these will be nonempty.
  Directory _path;
  XML _data;

  using LoadFunction = void (CDataLoader::*)(XmlReader &);
  void loadFromAllFiles(LoadFunction load);

  using FilesList = std::set<std::string>;
  FilesList findDataFiles() const;
  FilesList _files;
};

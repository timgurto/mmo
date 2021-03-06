#ifndef SOUND_PROFILE_H
#define SOUND_PROFILE_H

#include <map>
#include <string>
#include <vector>

#include "../types.h"

class SoundVariants;
class SoundsRecord;
struct Mix_Chunk;
class Client;

typedef std::string SoundType;
typedef int Channel;
const Channel NO_CHANNEL = -1;

class SoundProfile {
  std::string _id;
  ms_t _period;

  std::map<SoundType, SoundVariants> _sounds;

  static SoundsRecord loopingSounds;

  Channel checkAndPlaySound(const Client &client, const SoundType &type,
                            bool loop) const;

 public:
  SoundProfile(const std::string &id);

  void period(ms_t p) { _period = p; }
  ms_t period() const { return _period; }

  bool operator<(const SoundProfile &rhs) const;

  void add(const SoundType &type, std::string &filename);
  void playOnce(const Client &client, const SoundType &type) const;
  void startLooping(const Client &client, const SoundType &type,
                    const void *source) const;
  void stopLooping(const SoundType &type, const void *source) const;
};

class SoundVariants {
  std::vector<Mix_Chunk *> _variants;

 public:
  void add(Mix_Chunk *sound) { _variants.push_back(sound); }
  Mix_Chunk *choose() const;
};

// A mapping of channel -> sound source/type
class SoundsRecord {
  struct Entry {
    SoundType type;
    const void *source;

   public:
    Entry(const SoundType &type, const void *source)
        : type(type), source(source) {}
    static const Entry BLANK;
  };

  std::vector<Entry> _record;

 public:
  SoundsRecord();
  void set(const SoundType &type, const void *source, Channel channel);
  void unset(Channel channel);
  Channel SoundsRecord::getChannel(const std::string &type,
                                   const void *source) const;
};

#endif

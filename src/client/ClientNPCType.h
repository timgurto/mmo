#ifndef CLIENT_NPC_TYPE_H
#define CLIENT_NPC_TYPE_H

#include "ClientObject.h"

class SoundProfile;

class ClientNPCType : public ClientObjectType {
    Texture _corpseImage, _corpseHighlightImage;
    const SoundProfile *_sounds;

public:
    ClientNPCType(const std::string &id, health_t maxHealth);
    virtual ~ClientNPCType() override{}

    void corpseImage(const std::string &filename);
    const Texture &corpseImage() const { return _corpseImage; }
    const Texture &corpseHighlightImage() const { return _corpseHighlightImage; }
    void sounds(const std::string &id);
    const SoundProfile *sounds() const { return _sounds; }

    virtual char classTag() const override { return 'n'; }
};

#endif

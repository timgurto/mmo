#ifndef MESSAGE_CODES_H
#define MESSAGE_CODES_H

const char MSG_START = '\002',  // STX
    MSG_END = '\003',           // ETX
    MSG_DELIM = '\037';         // US

enum MessageCode {
  // clang-format off

  // Client -> server

  // A ping, to measure latency and reassure the server
  // Arguments: time sent
  CL_PING,

  // "I'm an existing user.  My details are ... and my client version is ..."
  // This has the effect of registering the user with the server.
  // Arguments: username, password hash, version
  CL_LOGIN_EXISTING,

  // "I'm new; my details are ... and I'm running client version ...
  // This has the effect of creating the account, then registering the user with the server.
  // Arguments: username, password hash, class, version
  CL_LOGIN_NEW,

  // "How long have I played?"
  CL_REQUEST_TIME_PLAYED,

  // "My location has changed, and is now ..."
  // Arguments: x, y
  CL_LOCATION,

  // Cancel user's current action
  CL_CANCEL_ACTION,

  // "I want to craft using recipe ..."
  // Arguments: id
  CL_CRAFT,

  // "I want to construct the item in inventory slot ..., at location ..."
  // Arguments: slot, x, y
  CL_CONSTRUCT_ITEM,

  // "I want to construct object ..., at location ..."
  // Arguments: id, x, y
  CL_CONSTRUCT,

  // "I want to pick up an object"
  // Arguments: serial
  CL_GATHER,

  // "I want to deconstruct an object"
  // Arguments: serial
  CL_DECONSTRUCT,

  // "I want to demolish an object"
  // Arguments: serial
  CL_DEMOLISH,

  // "I want to trade using merchant slot ... in object ..."
  // Arguments: serial, slot
  CL_TRADE,

  // "I want to drop the item in object ...'s slot ..."
  // An object serial of 0 denotes the user's inventory.
  // Arguments: serial, slot
  CL_DROP,

  // "I want to swap the items in container slots ... and ...". 
  // An object serial of 0 denotes the user's inventory.
  // An object serial of 1 denotes the user's gear.
  // Arguments: serial1, slot1, serial2, slot2
  CL_SWAP_ITEMS,

  // "I want to take the item in container slot ..."
  // An object serial of 1 denotes the user's gear.
  // Arguments: serial, slot
  CL_TAKE_ITEM,

  // "I want to set object ...'s merchant slot ... to the following:
  // Sell ...x... for ...x..."
  // Arguments: serial, slot, ware, wareQty, price, priceQty
  CL_SET_MERCHANT_SLOT,

  // "I want to clear object ...'s merchant slot ..."
  // Arguments: serial, slot
  CL_CLEAR_MERCHANT_SLOT,

  // "I want to mount vehicle ..."
  // Arguments: serial
  CL_MOUNT,

  // "I want to dismount my vehicle, to location (..., ...)"
  // Arguments: x, y
  CL_DISMOUNT,

  // "I want to give my object ... to my city".
  // Arguments: serial
  CL_CEDE,

  // "I want to grant my city's object ... to citizen ...".
  // Arguments: serial, username
  CL_GRANT,

  // "I want to leave my city".
  CL_LEAVE_CITY,

  // "I'm targeting entity ..."
  // Arguments: serial
  CL_TARGET_ENTITY,

  // "I'm targeting player ..."
  // Arguments: username
  CL_TARGET_PLAYER,

  // "I have entity ... selected"
  // Arguments: serial
  CL_SELECT_ENTITY,

  // "I have player ... selected"
  // Arguments: username
  CL_SELECT_PLAYER,

  // "I want player ... to join my city"
  // Arguments: username
  CL_RECRUIT,

  // "I want to declare war on ..."
  // Arguments: name
  CL_DECLARE_WAR_ON_PLAYER,
  CL_DECLARE_WAR_ON_CITY,
  CL_DECLARE_WAR_ON_PLAYER_AS_CITY,
  CL_DECLARE_WAR_ON_CITY_AS_CITY,
  // "I want to propose peace with ..."
  // Arguments: other belligerent's name
  CL_SUE_FOR_PEACE_WITH_PLAYER,
  CL_SUE_FOR_PEACE_WITH_CITY,
  CL_SUE_FOR_PEACE_WITH_PLAYER_AS_CITY,
  CL_SUE_FOR_PEACE_WITH_CITY_AS_CITY,
  // "I proposed peace with ... but changed my mind"
  // Arguments: other belligerent's name
  CL_CANCEL_PEACE_OFFER_TO_PLAYER,
  CL_CANCEL_PEACE_OFFER_TO_CITY,
  CL_CANCEL_PEACE_OFFER_TO_PLAYER_AS_CITY,
  CL_CANCEL_PEACE_OFFER_TO_CITY_AS_CITY,
  // "I accept ...'s peace offer"
  // Arguments: other belligerent's name
  CL_ACCEPT_PEACE_OFFER_WITH_PLAYER,
  CL_ACCEPT_PEACE_OFFER_WITH_CITY,
  CL_ACCEPT_PEACE_OFFER_WITH_PLAYER_AS_CITY,
  CL_ACCEPT_PEACE_OFFER_WITH_CITY_AS_CITY,

  // "I want to perform object ...'s action with argument ..."
  // Arguments: serial, textArg
  CL_PERFORM_OBJECT_ACTION,

  // I want to take talent ...
  // Arguments: talent name
  CL_TAKE_TALENT,

  // I want to unlearn all of my talents
  CL_UNLEARN_TALENTS,

  // Cast a spell
  // Arguments: spell ID
  CL_CAST,

  // Cast a spell by using an item
  // Arguments: inventory slot containing the item
  CL_CAST_ITEM,

  // Start a quest
  // Arguments: quest ID, start-object serial
  CL_ACCEPT_QUEST,

  // Complete a quest
  // Arguments: quest ID, end-object serial
  CL_COMPLETE_QUEST,

  // I no longer want to be on a quest
  // Arguments: questID
  CL_ABANDON_QUEST,

  // "I want to say ... to everybody". 
  // Arguments: message
  CL_SAY,

  // "I want to say ... to ...". 
  // Arguments: username, message
  CL_WHISPER,



  // Server -> client
    
  // A reply to a ping from a client
  // Arguments: time original was sent, time of reply
  SV_PING_REPLY,

  // The client has been successfully registered
  SV_WELCOME,

  // You have played for ... seconds
  SV_TIME_PLAYED,

  // A user has connected.  Broadcast to all players.
  // Arguments: username
  SV_USER_CONNECTED,

  // These ... users are already online: ..., ..., ...
  // Arguments: count, name1, name2, name3, ...
  SV_USERS_ALREADY_ONLINE,

  // A user has disconnected. Broadcast to all players.
  // Arguments: username
  SV_USER_DISCONNECTED,

  // A user has moved far away from you, and you will stop getting updates from him.
  // Arguments: username
  SV_USER_OUT_OF_RANGE,

  // An object has moved far away from you, and you will stop getting updates from him.
  // Arguments: serial
  SV_OBJECT_OUT_OF_RANGE,

  // The location of a user
  // Arguments: username, x, y
  SV_LOCATION,

  // The user's spawn point has changed
  SV_SET_SPAWN,

  // User ... is at location ..., and moved there instantly. (Used for respawning)
  // Arguments: username, x, y
  SV_LOCATION_INSTANT_USER,

  // Object #... is at location ..., and moved there instantly. (Used for respawning)
  // Arguments: serial, x, y
  SV_LOCATION_INSTANT_OBJECT,

  // An item is in the user's inventory, or a container object
  // Arguments: serial, slot, ID, quantity
  SV_INVENTORY,

  // A client received something.  Signal for aesthetics (e.g., floating combat text); the more
  // critical message to update client state is SV_INVENTORY.
  // Arguments: ID, quantity
  SV_RECEIVED_ITEM,

  // The details of an object
  // Arguments: serial, x, y, type
  SV_OBJECT,

  // An object has been removed
  // Arguments: serial
  SV_REMOVE_OBJECT,

  // An NPC has this level
  // Arguments: serial, level
  SV_NPC_LEVEL,

  // Details of an object's merchant slot
  // Arguments: serial, slot, ware, wareQty, price, priceQty
  SV_MERCHANT_SLOT,

  // The health of an NPC
  // Arguments: serial, health
  SV_ENTITY_HEALTH,

  // The energy of an NPC
  // Arguments: serial, energy
  SV_ENTITY_ENERGY,

  // The location of an object
  // Arguments: serial, x, y
  SV_OBJECT_LOCATION,

  // An object is transforming
  // Arguments: serial, remaining
  SV_TRANSFORM_TIME,

  // The user has begun an action
  // Arguments: time
  SV_ACTION_STARTED,

  // The user has completed an action
  SV_ACTION_FINISHED,

  // A user's class and level
  // Arguments: username, classID, level
  SV_CLASS,

  // A user's gear
  // Arguments: username, slot, id
  SV_GEAR,

  // The recipes a user knows
  // Arguments: quantity, id1, id2, ...
  SV_RECIPES,

  // New recipes a user has just learned
  // Arguments: quantity, id1, id2, ...
  SV_NEW_RECIPES,

  // The constructions a user knows
  // Arguments: quantity, id1, id2, ...
  SV_CONSTRUCTIONS,

  // New constructions a user has just learned
  // Arguments: quantity, id1, id2, ...
  SV_NEW_CONSTRUCTIONS,

  // The user has just joined a city
  // Arguments: cityName
  SV_JOINED_CITY,

  // A user is a member of a city
  // Arguments: username, cityName
  SV_IN_CITY,

  // A user is not a member of a city
  // Arguments: username
  SV_NO_CITY,

  SV_CITY_FOUNDED, // Arguments: founder name, city name
  SV_CITY_DESTROYED, // Arguments: city name

  // A user is a king
  // Arguments: username
  SV_KING,

  // An NPC hit a player
  // Arguments: serial, username
  SV_ENTITY_HIT_PLAYER,
    
  // A player hit an NPC
  // Arguments: username, serial
  SV_PLAYER_HIT_ENTITY,
    
  // A player hit an another player
  // Arguments: attacker's username, defender's username
  SV_PLAYER_HIT_PLAYER,

  // The outcome of a spellcast or ranged attack.  This is purely so that the client can
  // illustrate it.
  SV_SPELL_HIT, // spell ID, from x, from y, to x, to y
  SV_SPELL_MISS, // spell ID, from x, from y, to x, to y
  SV_RANGED_NPC_HIT, // npc ID, from x, from y, to x, to y
  SV_RANGED_NPC_MISS, // npc ID, from x, from y, to x, to y
  SV_RANGED_WEAPON_HIT, // weapon ID, from x, from y, to x, to y
  SV_RANGED_WEAPON_MISS, // weapon ID, from x, from y, to x, to y

  // Entity was hit by something.
  // Arguments: username/serial
  SV_PLAYER_WAS_HIT,
  SV_ENTITY_WAS_HIT,

  // Something has a noteworthy outcome. Show it.
  // Arguments: x, y
  SV_SHOW_MISS_AT,
  SV_SHOW_DODGE_AT,
  SV_SHOW_BLOCK_AT,
  SV_SHOW_CRIT_AT,

  // Something was buffed/debuffed
  // Arguments: serial/username, buff ID
  SV_ENTITY_GOT_BUFF,
  SV_PLAYER_GOT_BUFF,
  SV_ENTITY_GOT_DEBUFF,
  SV_PLAYER_GOT_DEBUFF,
  // Something lost a buff/debuff
  // Arguments: serial/username, buff ID
  SV_ENTITY_LOST_BUFF,
  SV_PLAYER_LOST_BUFF,
  SV_ENTITY_LOST_DEBUFF,
  SV_PLAYER_LOST_DEBUFF,
  // Your buff/debuff has a different time remaining than you would expect
  // Arguments: (de)buff ID, time remaining (ms)
  SV_REMAINING_BUFF_TIME,
  SV_REMAINING_DEBUFF_TIME,

  // You died
  SV_YOU_DIED,

  // You know the following ... spells: ..., ..., etc.
  // Arguments: count, id1, id2, id3, ...
  SV_KNOWN_SPELLS,

  // You just learned spell ...
  // Arguments: ID
  SV_LEARNED_SPELL,

  // You just unlearned spell ...
  // Arguments: ID
  SV_UNLEARNED_SPELL,

  // How many points a player has in a talent
  // Arguments: name, points
  SV_TALENT,

  // The user died and lost a talet point
  // Arguments: talent name
  SV_LOST_TALENT,

  // How many total points a player has in a talent tree
  // Arguments: tree name, points
  SV_POINTS_IN_TREE,

  // The user has no talent points allocated
  SV_NO_TALENTS,

  // A spell is cooling down
  // Arguments: spellID, time left in ms
  SV_SPELL_COOLING_DOWN,

  // An object has an owner
  // Arguments: serial, type ("user"|"city"), name
  SV_OWNER,

  // An object is being gathered from
  SV_GATHERING_OBJECT,

  // An object is not being gathered from
  SV_NOT_GATHERING_OBJECT,

  // A vehicle has a driver.
  // Arguments: serial, username
  SV_MOUNTED,

  // A vehicle is no longer being driven.
  // Arguments: serial, username
  SV_UNMOUNTED,

  // The remaining materials required to construct an object
  // Arguments: serial, n, id1, quantity1, id2, quantity2, ...
  SV_CONSTRUCTION_MATERIALS,

  // The user's stats
  // Arguments: armor, max health, max energy, hps, eps,
  // hit, crit, critResist, dodge, block, blockValue,
  // magicDamage, physicalDamage, healing,
  // airResist, earthResist, fireResist, waterResist, attack time, speed
  SV_YOUR_STATS,

  // The user's XP
  // Arguments: XP, maxXP
  SV_XP,

  // The user received new XP
  // Arguments: XP
  SV_XP_GAIN,

  // A player leveled up
  // Arguments: username
  SV_LEVEL_UP,

  // A user's health value
  // Arguments: username, hp
  SV_PLAYER_HEALTH,

  // An entity has been damaged/healed.  For aesthetics, e.g., floating combat text.
  // Arguments: username, amount
  SV_PLAYER_DAMAGED,
  SV_PLAYER_HEALED,
  // Arguments: serial, amount
  SV_OBJECT_DAMAGED,
  SV_OBJECT_HEALED,

  // A user's energy value
  // Arguments: username, energy
  SV_PLAYER_ENERGY,

  // A user's max health
  // Arguments: username, max health
  SV_MAX_HEALTH,

  // A user's max energy
  // Arguments: username, max energy
  SV_MAX_ENERGY,

  // "You are at war with ..."
  // Arguments: name
  SV_AT_WAR_WITH_PLAYER,
  SV_AT_WAR_WITH_CITY,
  SV_YOUR_CITY_AT_WAR_WITH_PLAYER,
  SV_YOUR_CITY_AT_WAR_WITH_CITY,
  // "You have sued for peace with ..."
  // Arguments: name
  SV_YOU_PROPOSED_PEACE_TO_PLAYER,
  SV_YOU_PROPOSED_PEACE_TO_CITY,
  SV_YOUR_CITY_PROPOSED_PEACE_TO_PLAYER,
  SV_YOUR_CITY_PROPOSED_PEACE_TO_CITY,
  // "... has sued for peace with you"
  // Arguments: name
  SV_PEACE_WAS_PROPOSED_TO_YOU_BY_PLAYER,
  SV_PEACE_WAS_PROPOSED_TO_YOU_BY_CITY,
  SV_PEACE_WAS_PROPOSED_TO_YOUR_CITY_BY_PLAYER,
  SV_PEACE_WAS_PROPOSED_TO_YOUR_CITY_BY_CITY,
  // "You cancelled your peace offer to ..."
  // Arguments: name
  SV_YOU_CANCELED_PEACE_OFFER_TO_PLAYER,
  SV_YOU_CANCELED_PEACE_OFFER_TO_CITY,
  SV_YOUR_CITY_CANCELED_PEACE_OFFER_TO_PLAYER,
  SV_YOUR_CITY_CANCELED_PEACE_OFFER_TO_CITY,
  // "Your peace offer from ... was canceled"
  // Arguments: name
  SV_PEACE_OFFER_TO_YOU_FROM_PLAYER_WAS_CANCELED,
  SV_PEACE_OFFER_TO_YOU_FROM_CITY_WAS_CANCELED,
  SV_PEACE_OFFER_TO_YOUR_CITY_FROM_PLAYER_WAS_CANCELED,
  SV_PEACE_OFFER_TO_YOUR_CITY_FROM_CITY_WAS_CANCELED,
  // "You are at peace with ..."
  // Arguments: name
  SV_AT_PEACE_WITH_PLAYER,
  SV_AT_PEACE_WITH_CITY,
  SV_YOUR_CITY_IS_AT_PEACE_WITH_PLAYER,
  SV_YOUR_CITY_IS_AT_PEACE_WITH_CITY,

  // A change in state for a quest
  // Arguments: questID
  SV_QUEST_CAN_BE_STARTED,
  SV_QUEST_IN_PROGRESS,
  SV_QUEST_CAN_BE_FINISHED,
  SV_QUEST_COMPLETED,

  // For aesthetics only.  Quest-completion aesthetics are handled by SV_QUEST_COMPLETED.
  SV_QUEST_ACCEPTED,

  // Progress on a quest objective
  // Arguments: questID, objective index, progress
  SV_QUEST_PROGRESS,

  // "User ... has said ...".
  // Arguments: username, message
  SV_SAY,

  // "User ... has said ... to you".
  // Arguments: username, message
  SV_WHISPER,



  // Errors and warnings
  // Warning: the user is unable to perform a reasonable action, for X reason
  // Error: the user should not have been able to attempt that action, or something went wrong.

  // Connection
  WARNING_WRONG_VERSION, // The client version differs from the server version. Argument: server version
  WARNING_DUPLICATE_USERNAME, // The client has attempted to connect with a username already in use
  WARNING_INVALID_USERNAME, // The client has attempted to connect with an invalid username
  WARNING_SERVER_FULL, // There is no room for more clients
  WARNING_USER_DOESNT_EXIST, // User tried to log in to an account that doesn't exist
  WARNING_NAME_TAKEN, // User tried to create an account with an unavailable name
  WARNING_WRONG_PASSWORD, // The attempted password doesn't match the existing username

  // Merchant objects
  ERROR_NOT_MERCHANT, // The user tried to perform a merchant function on a non-merchant object
  ERROR_INVALID_MERCHANT_SLOT, // The user tried to perform a merchant function on an invalid merchant slot
  WARNING_NO_WARE, // The merchant has no wares in stock to sell the user
  WARNING_NO_PRICE, // The user cannot afford the price of a merchant exchange
  WARNING_MERCHANT_INVENTORY_FULL, // The merchant object does not have enough inventory space to trade with the user

  // Talents and spells
  ERROR_INVALID_TALENT, // You tried to learn an invalid spell
  WARNING_MISSING_ITEMS_FOR_TALENT, // You tried to learn a talent which you can't afford
  WARNING_MISSING_REQ_FOR_TALENT, // You do not meet the requirements for that talent
  ERROR_ALREADY_KNOW_SPELL, // You tried to learn a spell that you already know
  WARNING_NO_TALENT_POINTS, // You don't have any talent points left to allocate
  ERROR_DONT_KNOW_SPELL, // You tried to cast a spell that you don't know
  WARNING_INVALID_SPELL_TARGET, // You can't cast that spell at that target
  WARNING_NOT_ENOUGH_ENERGY, // You don't have enough energy to cast that
  ERROR_CANNOT_CAST_ITEM, // That item doesn't cast a spell

  // Vehicles
  ERROR_NOT_VEHICLE, // The user tried to mount a non-vehicle object.
  WARNING_VEHICLE_OCCUPIED, // The user tried to perform an action on an occupied vehicle
  WARNING_NO_VEHICLE, // The user tried to perform an action on an occupied vehicle

  // Crafting
  WARNING_NEED_MATERIALS, // The user does not have enough materials to craft an item
  ERROR_INVALID_ITEM, // The user tried to craft an item that does not exist
  ERROR_CANNOT_CRAFT, // The user referred to a nonexistent item
  ERROR_UNKNOWN_RECIPE, // The user tried to craft using a recipe he doesn't know
         
  // Construction
  ERROR_INVALID_OBJECT, // The user tried to construct an object type that doesn't exist
  ERROR_UNKNOWN_CONSTRUCTION, // The user tried to construct something that he doesn't know about
  WARNING_WRONG_MATERIAL, // The user tried to add the wrong building material to a site
  ERROR_UNDER_CONSTRUCTION, // The user tried to use an object that is still under construction
  ERROR_UNBUILDABLE, // The user tried to construct an object that cannot be constructed
  ERROR_CANNOT_CONSTRUCT, // The user tried to construct an item that cannot be constructed
  WARNING_UNIQUE_OBJECT, // The user tried to construct a unique object that already exists in the world
  WARNING_PLAYER_UNIQUE_OBJECT, // The user tried to build a second user-unique object. Arguments: category

  // Cities
  ERROR_NOT_IN_CITY, // The user tried to perform a city action when not in a city.
  ERROR_KING_CANNOT_LEAVE_CITY, // The user tried to leave a city while being its king.
  ERROR_ALREADY_IN_CITY, // The user tried to recruit a citizen of another city.
  WARNING_YOU_ARE_ALREADY_IN_CITY, // The user is already in a city and cannot found a new one.
  ERROR_NOT_A_KING, // Only a king can perform that action

  // Objects
  WARNING_DOESNT_EXIST, // The user tried to perform an action on an object that doesn't exist
  ERROR_CANNOT_DECONSTRUCT, // The user tried to deconstruct an object that cannot be deconstructed
  ERROR_DAMAGED_OBJECT, // That action cannot be performed on a damaged object
  ERROR_CANNOT_CEDE, // The user tried to cede an uncedable object
  ERROR_NO_ACTION, // The user tried to perform an action with an object that has none.

  // Inventory and containers
  WARNING_INVENTORY_FULL, // The user cannot receive an item because his inventory is full
  ERROR_EMPTY_SLOT, // The user tried to manipulate an empty inventory slot
  ERROR_INVALID_SLOT, // The user attempted to manipulate an out-of-range inventory slot
  WARNING_NOT_EMPTY, // The object cannot be removed because it has an inventory of items
  ERROR_NPC_SWAP, // The user tried to put an item into an NPC
  ERROR_TAKE_SELF, // The user tried to take an item from himself
  ERROR_NO_INVENTORY, // The user tried to manipulate an object's non-existent inventory
  WARNING_OUT_OF_AMMO, // The user tried to use a weapon that uses ammo, without having said ammo.  Arguments: ammo ID

  // War
  ERROR_ATTACKED_PEACFUL_PLAYER, // The user tried to attack a player without being at war with him
  ERROR_ALREADY_AT_WAR, // The user tried to declare war on somebody with whom they are already at war.

  // Misc
  WARNING_TOO_FAR, // The user is too far away to perform an action
  WARNING_NO_PERMISSION, // The user does not have permission to perform an action
  ERROR_INVALID_USER, // That user doesn't exist
  WARNING_NEED_TOOLS, // The user does not have the tools required to craft an item
  WARNING_ACTION_INTERRUPTED, // The user was unable to complete an action
  WARNING_ITEM_TAG_NEEDED, // The user tried to perform an action but does not have the requisite item. Arguments: requiredItemTag
  WARNING_ITEM_NEEDED, // The user tried to perform an action but does not have the requisite item.
  WARNING_BLOCKED, // The user tried to perform an action at an occupied location
  ERROR_TARGET_DEAD, // The NPC is dead
  ERROR_NOT_GEAR, // The user tried to equip an item into a gear slot with which it isn't compatible.
  WARNING_STUNNED, // The user can't perform an action because he is stunned



  // Debug requests

  // "Give me a full stack of ..."
  // Arguments: id
  DG_GIVE,

  // "Unlock everything for me"
  DG_UNLOCK,

  // "Give me enough XP to level up"
  DG_LEVEL,

  // "Teleport me to ..."
  // Arguments: x, y
  DG_TELEPORT,

  // End the tutorial
  DG_SKIP_TUTORIAL,

  // "Teach me all spells"
  DG_SPELLS,

  // "Kill me"
  DG_DIE,



  // Messages used in tests

  // Tell the client to send the message back to the server; used for remote clients.
  // Arguments: message code, args
  TST_SEND_THIS_BACK,



  NO_CODE
  // clang-format on
};

#endif

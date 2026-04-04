#include <stdbool.h>

// FMOD Studio C++ wrapper compatibility layer.
//
// Some fmodpp archives used with VitaSDK miss several C++ mangled methods
// (especially const-qualified wrappers) that dynlib.c binds explicitly.
// Provide weak, local implementations for the missing symbols and forward to
// available FMOD_Studio_* C API imports from libfmodstudio_stub.

extern "C" {
int FMOD_Studio_Bus_SetVolume(void *bus, float volume);
int FMOD_Studio_EventInstance_GetUserData(void *eventinstance, void **userdata);
int FMOD_Studio_EventInstance_GetParameter(void *eventinstance, const char *name, void **parameter);
int FMOD_Studio_EventInstance_GetChannelGroup(void *eventinstance, void **group);
int FMOD_Studio_EventInstance_GetPlaybackState(void *eventinstance, int *state);
int FMOD_Studio_EventInstance_GetParameterCount(void *eventinstance, int *count);
int FMOD_Studio_EventInstance_GetParameterByIndex(void *eventinstance, int index, void **parameter);
int FMOD_Studio_EventInstance_GetTimelinePosition(void *eventinstance, int *position);
bool FMOD_Studio_EventInstance_IsValid(void *eventinstance);

int FMOD_Studio_EventDescription_CreateInstance(void *eventdescription, void **instance);
int FMOD_Studio_EventDescription_GetUserProperty(void *eventdescription, const char *name, void *property);
int FMOD_Studio_EventDescription_GetParameterCount(void *eventdescription, int *count);
int FMOD_Studio_EventDescription_GetParameterByIndex(void *eventdescription, int index, void *parameter);
int FMOD_Studio_EventDescription_GetSampleLoadingState(void *eventdescription, int *state);
int FMOD_Studio_EventDescription_Is3D(void *eventdescription, bool *is3d);
int FMOD_Studio_EventDescription_GetID(void *eventdescription, void *guid);
bool FMOD_Studio_EventDescription_IsValid(void *eventdescription);
int FMOD_Studio_EventDescription_GetLength(void *eventdescription, int *length);
int FMOD_Studio_EventDescription_IsOneshot(void *eventdescription, bool *oneshot);

int FMOD_Studio_ParameterInstance_GetDescription(void *parameterinstance, void *description);
bool FMOD_Studio_ParameterInstance_IsValid(void *parameterinstance);

int FMOD_Studio_Bus_GetChannelGroup(void *bus, void **group);
int FMOD_Studio_Bus_GetID(void *bus, void *guid);
bool FMOD_Studio_Bus_IsValid(void *bus);

int FMOD_Studio_Bank_GetBusList(void *bank, void **array, int capacity, int *count);
int FMOD_Studio_Bank_GetBusCount(void *bank, int *count);
int FMOD_Studio_Bank_GetEventList(void *bank, void **array, int capacity, int *count);
int FMOD_Studio_Bank_GetEventCount(void *bank, int *count);
int FMOD_Studio_Bank_GetLoadingState(void *bank, int *state);
bool FMOD_Studio_Bank_IsValid(void *bank);

int FMOD_Studio_System_GetBusByID(void *system, const void *guid, void **bus);
int FMOD_Studio_System_LookupPath(void *system, const void *guid, char *path, int size, int *retrieved);
int FMOD_Studio_System_GetBankList(void *system, void **array, int capacity, int *count);
int FMOD_Studio_System_GetBankCount(void *system, int *count);
int FMOD_Studio_System_GetEventByID(void *system, const void *guid, void **eventdescription);
int FMOD_Studio_System_GetSoundInfo(void *system, const char *key, void *soundinfo);
int FMOD_Studio_System_GetLowLevelSystem(void *system, void **lowlevel);
}

#define FMOD_ERR_UNSUPPORTED 30

#define FMODPP_ALIAS(sym, ret, args, body) \
  extern "C" __attribute__((weak)) ret sym args __asm__(#sym); \
  extern "C" __attribute__((weak)) ret sym args body

// Missing non-const wrappers.
FMODPP_ALIAS(_ZN4FMOD6Studio3Bus13setFaderLevelEf, int, (void *self, float v), {
  return FMOD_Studio_Bus_SetVolume(self, v);
});

FMODPP_ALIAS(_ZN4FMOD6Studio11CueInstance7triggerEv, int, (void *self), {
  (void)self;
  // No matching C API symbol is available in Vita's libfmodstudio_stub.
  return FMOD_ERR_UNSUPPORTED;
});

// Missing const-qualified wrappers.
FMODPP_ALIAS(_ZNK4FMOD6Studio13EventInstance11getUserDataEPPv, int, (const void *self, void **ud), {
  return FMOD_Studio_EventInstance_GetUserData((void *)self, ud);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio13EventInstance12getParameterEPKcPPNS0_17ParameterInstanceE, int, (const void *self, const char *n, void **p), {
  return FMOD_Studio_EventInstance_GetParameter((void *)self, n, p);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio13EventInstance13getCueByIndexEiPPNS0_11CueInstanceE, int, (const void *self, int idx, void **cue), {
  (void)self;
  (void)idx;
  if (cue) *cue = 0;
  return FMOD_ERR_UNSUPPORTED;
});
FMODPP_ALIAS(_ZNK4FMOD6Studio13EventInstance15getChannelGroupEPPNS_12ChannelGroupE, int, (const void *self, void **g), {
  return FMOD_Studio_EventInstance_GetChannelGroup((void *)self, g);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio13EventInstance16getPlaybackStateEP26FMOD_STUDIO_PLAYBACK_STATE, int, (const void *self, int *s), {
  return FMOD_Studio_EventInstance_GetPlaybackState((void *)self, s);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio13EventInstance17getParameterCountEPi, int, (const void *self, int *c), {
  return FMOD_Studio_EventInstance_GetParameterCount((void *)self, c);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio13EventInstance19getParameterByIndexEiPPNS0_17ParameterInstanceE, int, (const void *self, int i, void **p), {
  return FMOD_Studio_EventInstance_GetParameterByIndex((void *)self, i, p);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio13EventInstance19getTimelinePositionEPi, int, (const void *self, int *p), {
  return FMOD_Studio_EventInstance_GetTimelinePosition((void *)self, p);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio13EventInstance7isValidEv, bool, (const void *self), {
  return FMOD_Studio_EventInstance_IsValid((void *)self);
});

FMODPP_ALIAS(_ZNK4FMOD6Studio16EventDescription14createInstanceEPPNS0_13EventInstanceE, int, (const void *self, void **i), {
  return FMOD_Studio_EventDescription_CreateInstance((void *)self, i);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio16EventDescription15getUserPropertyEPKcP25FMOD_STUDIO_USER_PROPERTY, int, (const void *self, const char *n, void *p), {
  return FMOD_Studio_EventDescription_GetUserProperty((void *)self, n, p);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio16EventDescription17getParameterCountEPi, int, (const void *self, int *c), {
  return FMOD_Studio_EventDescription_GetParameterCount((void *)self, c);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio16EventDescription19getParameterByIndexEiP33FMOD_STUDIO_PARAMETER_DESCRIPTION, int, (const void *self, int i, void *p), {
  return FMOD_Studio_EventDescription_GetParameterByIndex((void *)self, i, p);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio16EventDescription21getSampleLoadingStateEP25FMOD_STUDIO_LOADING_STATE, int, (const void *self, int *s), {
  return FMOD_Studio_EventDescription_GetSampleLoadingState((void *)self, s);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio16EventDescription4is3DEPb, int, (const void *self, bool *b), {
  return FMOD_Studio_EventDescription_Is3D((void *)self, b);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio16EventDescription5getIDEP9FMOD_GUID, int, (const void *self, void *g), {
  return FMOD_Studio_EventDescription_GetID((void *)self, g);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio16EventDescription7isValidEv, bool, (const void *self), {
  return FMOD_Studio_EventDescription_IsValid((void *)self);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio16EventDescription9getLengthEPi, int, (const void *self, int *l), {
  return FMOD_Studio_EventDescription_GetLength((void *)self, l);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio16EventDescription9isOneshotEPb, int, (const void *self, bool *o), {
  return FMOD_Studio_EventDescription_IsOneshot((void *)self, o);
});

FMODPP_ALIAS(_ZNK4FMOD6Studio17ParameterInstance14getDescriptionEP33FMOD_STUDIO_PARAMETER_DESCRIPTION, int, (const void *self, void *d), {
  return FMOD_Studio_ParameterInstance_GetDescription((void *)self, d);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio17ParameterInstance7isValidEv, bool, (const void *self), {
  return FMOD_Studio_ParameterInstance_IsValid((void *)self);
});

FMODPP_ALIAS(_ZNK4FMOD6Studio3Bus15getChannelGroupEPPNS_12ChannelGroupE, int, (const void *self, void **g), {
  return FMOD_Studio_Bus_GetChannelGroup((void *)self, g);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio3Bus5getIDEP9FMOD_GUID, int, (const void *self, void *g), {
  return FMOD_Studio_Bus_GetID((void *)self, g);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio3Bus7isValidEv, bool, (const void *self), {
  return FMOD_Studio_Bus_IsValid((void *)self);
});

FMODPP_ALIAS(_ZNK4FMOD6Studio4Bank10getBusListEPPNS0_3BusEiPi, int, (const void *self, void **a, int cap, int *c), {
  return FMOD_Studio_Bank_GetBusList((void *)self, a, cap, c);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio4Bank11getBusCountEPi, int, (const void *self, int *c), {
  return FMOD_Studio_Bank_GetBusCount((void *)self, c);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio4Bank12getEventListEPPNS0_16EventDescriptionEiPi, int, (const void *self, void **a, int cap, int *c), {
  return FMOD_Studio_Bank_GetEventList((void *)self, a, cap, c);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio4Bank13getEventCountEPi, int, (const void *self, int *c), {
  return FMOD_Studio_Bank_GetEventCount((void *)self, c);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio4Bank15getLoadingStateEP25FMOD_STUDIO_LOADING_STATE, int, (const void *self, int *s), {
  return FMOD_Studio_Bank_GetLoadingState((void *)self, s);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio4Bank7isValidEv, bool, (const void *self), {
  return FMOD_Studio_Bank_IsValid((void *)self);
});

FMODPP_ALIAS(_ZNK4FMOD6Studio6System10getBusByIDEPK9FMOD_GUIDPPNS0_3BusE, int, (const void *self, const void *g, void **b), {
  return FMOD_Studio_System_GetBusByID((void *)self, g, b);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio6System10lookupPathEPK9FMOD_GUIDPciPi, int, (const void *self, const void *g, char *p, int sz, int *r), {
  return FMOD_Studio_System_LookupPath((void *)self, g, p, sz, r);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio6System11getBankListEPPNS0_4BankEiPi, int, (const void *self, void **a, int cap, int *c), {
  return FMOD_Studio_System_GetBankList((void *)self, a, cap, c);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio6System12getBankCountEPi, int, (const void *self, int *c), {
  return FMOD_Studio_System_GetBankCount((void *)self, c);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio6System12getEventByIDEPK9FMOD_GUIDPPNS0_16EventDescriptionE, int, (const void *self, const void *g, void **e), {
  return FMOD_Studio_System_GetEventByID((void *)self, g, e);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio6System12getSoundInfoEPKcP22FMOD_STUDIO_SOUND_INFO, int, (const void *self, const char *k, void *i), {
  return FMOD_Studio_System_GetSoundInfo((void *)self, k, i);
});
FMODPP_ALIAS(_ZNK4FMOD6Studio6System17getLowLevelSystemEPPNS_6SystemE, int, (const void *self, void **l), {
  return FMOD_Studio_System_GetLowLevelSystem((void *)self, l);
});

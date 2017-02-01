/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __ANDROID_TRACK_PLAYER_BASE_H__
#define __ANDROID_TRACK_PLAYER_BASE_H__

#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <audiomanager/IPlayer.h>
#include <audiomanager/AudioManager.h>
#include <audiomanager/IAudioManager.h>
#include "utils/RefBase.h"


namespace android {

class TrackPlayerBase : public BnPlayer
{
public:
    explicit TrackPlayerBase();
    virtual ~TrackPlayerBase();

    void init(AudioTrack* pat, player_type_t playerType, audio_usage_t usage);
    void destroy();

    //IPlayer implementation
    virtual void start();
    virtual void pause();
    virtual void stop();
    virtual void setVolume(float vol);
    virtual void setPan(float pan);
    virtual void setStartDelayMs(int32_t delayMs);

    virtual status_t onTransact(
                uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags);

    //FIXME move to protected field, so far made public to minimize changes to AudioTrack logic
    sp<AudioTrack> mAudioTrack;

    //FIXME temporary method while some AudioTrack state is outside of this class
    void reportEvent(player_state_t event);

    void setSlPlayerVolume(float vl, float vr);

protected:
    // native interface to AudioService
    android::sp<android::IAudioManager> mAudioManager;

    // report events to AudioService
    void servicePlayerEvent(player_state_t event);
    void serviceReleasePlayer();

    // player interface ID, uniquely identifies the player in the system
    audio_unique_id_t mPIId;

    // mutex for IPlayer volume and pan, and SL volume
    Mutex mSettingsLock;
    // volume coming from the SL ES volume API
    float mSlPlayerVolumeL, mSlPlayerVolumeR;
    // volume multipliers coming from the IPlayer volume and pan controls
    float mPanMultiplierL, mPanMultiplierR;
    float mVolumeMultiplierL, mVolumeMultiplierR;

    DISALLOW_EVIL_CONSTRUCTORS(TrackPlayerBase);
};

} // namespace android

#endif /* __ANDROID_TRACK_PLAYER_BASE_H__ */

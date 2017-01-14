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

#include "sles_allinclusive.h"

#include <media/stagefright/foundation/ADebug.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <audiomanager/IPlayer.h>
#include <audiomanager/AudioManager.h>
#include <audiomanager/IAudioManager.h>
#include <binder/IServiceManager.h>
#include "utils/RefBase.h"

namespace android {

//--------------------------------------------------------------------------------------------------
TrackPlayerBase::TrackPlayerBase() : BnPlayer(),
        mPIId(PLAYER_PIID_INVALID)
{
    SL_LOGD("TrackPlayerBase::TrackPlayerBase()");
    // use checkService() to avoid blocking if audio service is not up yet
    sp<IBinder> binder = defaultServiceManager()->checkService(String16("audio"));
    if (binder == 0) {
        SL_LOGE("TrackPlayerBase(): binding to audio service failed, service up?");
    } else {
        mAudioManager = interface_cast<IAudioManager>(binder);
    }
}


TrackPlayerBase::~TrackPlayerBase() {
    SL_LOGV("TrackPlayerBase::~TrackPlayerBase()");
    destroy();
}

void TrackPlayerBase::init(AudioTrack* pat, player_type_t playerType, audio_usage_t usage) {
    mAudioTrack = pat;
    if (mAudioManager == 0) {
                SL_LOGE("AudioPlayer realize: no audio service, player will not be registered");
    } else {
        mPIId = mAudioManager->trackPlayer(playerType, usage, AUDIO_CONTENT_TYPE_UNKNOWN, this);
    }
}

void TrackPlayerBase::destroy() {
    if (mAudioTrack != 0) {
        mAudioTrack->stop();
        // Note that there may still be another reference in post-unlock phase of SetPlayState
        mAudioTrack.clear();
    }
    serviceReleasePlayer();
    if (mAudioManager != 0) {
        mAudioManager.clear();
    }
}

//------------------------------------------------------------------------------
void TrackPlayerBase::servicePlayerEvent(player_state_t event) {
    if (mAudioManager != 0) {
        mAudioManager->playerEvent(mPIId, event);
    }
}

void TrackPlayerBase::serviceReleasePlayer() {
    if (mAudioManager != 0) {
        mAudioManager->releasePlayer(mPIId);
    }
}

//FIXME temporary method while some AudioTrack state is outside of this class
void TrackPlayerBase::reportEvent(player_state_t event) {
    servicePlayerEvent(event);
}

//------------------------------------------------------------------------------
// Implementation of IPlayer
void TrackPlayerBase::start() {
    if (mAudioTrack != 0) {
        SL_LOGD("TrackPlayerBase::start() from IPlayer");
        //FIXME pipe this to SL player, not AudioTrack directly
        if (mAudioTrack->start() == NO_ERROR) {
            servicePlayerEvent(PLAYER_STATE_STARTED);
        }
    } else {
        SL_LOGD("TrackPlayerBase::start() no AudioTrack to start from IPlayer");
    }
}

void TrackPlayerBase::pause() {
    if (mAudioTrack != 0) {
        SL_LOGD("TrackPlayerBase::pause() from IPlayer");
        //FIXME pipe this to SL player, not AudioTrack directly
        mAudioTrack->pause();
        servicePlayerEvent(PLAYER_STATE_PAUSED);
    } else {
        SL_LOGD("TrackPlayerBase::pause() no AudioTrack to pause from IPlayer");
    }
}


void TrackPlayerBase::stop() {
    if (mAudioTrack != 0) {
        SL_LOGD("TrackPlayerBase::stop() from IPlayer");
        //FIXME pipe this to SL player, not AudioTrack directly
        mAudioTrack->stop();
        servicePlayerEvent(PLAYER_STATE_STOPPED);
    } else {
        SL_LOGD("TrackPlayerBase::stop() no AudioTrack to stop from IPlayer");
    }
}

void TrackPlayerBase::setVolume(float vol) {
    //FIXME combine SL player volume with IPlayer volume and pan
    if (mAudioTrack != 0) {
        SL_LOGD("TrackPlayerBase::setVolume() from IPlayer");
        mAudioTrack->setVolume(vol);
    } else {
        SL_LOGD("TrackPlayerBase::setVolume() no AudioTrack for volume control from IPlayer");
    }
}

status_t TrackPlayerBase::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    return BnPlayer::onTransact(code, data, reply, flags);
}

} // namespace android

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

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

namespace android {

//--------------------------------------------------------------------------------------------------
TrackPlayerBase::TrackPlayerBase() : BnPlayer(),
        mPIId(PLAYER_PIID_INVALID), mLastReportedEvent(PLAYER_STATE_UNKNOWN),
        mSlPlayerVolumeL(1.0f), mSlPlayerVolumeR(1.0f),
        mPanMultiplierL(1.0f), mPanMultiplierR(1.0f),
        mVolumeMultiplierL(1.0f), mVolumeMultiplierR(1.0f)
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

void TrackPlayerBase::setSlPlayerVolume(float vl, float vr) {
    float tl, tr = 1.0f;
    {
        Mutex::Autolock _l(mSettingsLock);
        mSlPlayerVolumeL = vl;
        mSlPlayerVolumeR = vr;
        tl = mSlPlayerVolumeL * mPanMultiplierL * mVolumeMultiplierL;
        tr = mSlPlayerVolumeR * mPanMultiplierR * mVolumeMultiplierR;
    }
    if (mAudioTrack != 0) {
        mAudioTrack->setVolume(tl, tr);
    }
}

//------------------------------------------------------------------------------
void TrackPlayerBase::servicePlayerEvent(player_state_t event) {
    if (mAudioManager != 0) {
        // only report state change
        Mutex::Autolock _l(mPlayerStateLock);
        if (event != mLastReportedEvent
                && mPIId != PLAYER_PIID_INVALID) {
            mLastReportedEvent = event;
            mAudioManager->playerEvent(mPIId, event);
        }
    }
}

void TrackPlayerBase::serviceReleasePlayer() {
    if (mAudioManager != 0
            && mPIId != PLAYER_PIID_INVALID) {
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
    float tl, tr = 1.0f;
    {
        Mutex::Autolock _l(mSettingsLock);
        mVolumeMultiplierL = vol;
        mVolumeMultiplierR = vol;
        tl = mSlPlayerVolumeL * mPanMultiplierL * mVolumeMultiplierL;
        tr = mSlPlayerVolumeR * mPanMultiplierR * mVolumeMultiplierR;
    }
    if (mAudioTrack != 0) {
        SL_LOGD("TrackPlayerBase::setVolume() from IPlayer");
        mAudioTrack->setVolume(tl, tr);
    } else {
        SL_LOGD("TrackPlayerBase::setVolume() no AudioTrack for volume control from IPlayer");
    }
}

void TrackPlayerBase::setPan(float pan) {
    float tl, tr = 1.0f;
    {
        Mutex::Autolock _l(mSettingsLock);
        pan = min(max(-1.0f, pan), 1.0f);
        if (pan >= 0.0f) {
            mPanMultiplierL = 1.0f - pan;
            mPanMultiplierR = 1.0f;
        } else {
            mPanMultiplierL = 1.0f;
            mPanMultiplierR = 1.0f + pan;
        }
        tl = mSlPlayerVolumeL * mPanMultiplierL * mVolumeMultiplierL;
        tr = mSlPlayerVolumeR * mPanMultiplierR * mVolumeMultiplierR;
    }
    if (mAudioTrack != 0) {
        SL_LOGD("TrackPlayerBase::setPan() from IPlayer");
        mAudioTrack->setVolume(tl, tr);
    } else {
        SL_LOGD("TrackPlayerBase::setPan() no AudioTrack for volume control from IPlayer");
    }
}

void TrackPlayerBase::setStartDelayMs(int32_t delayMs) {
    //FIXME not supported for SLES
    SL_LOGW("setStartDelay() is not supported in OpenSL ES");
}

void TrackPlayerBase::applyVolumeShaper(
        const sp<VolumeShaper::Configuration>& configuration,
        const sp<VolumeShaper::Operation>& operation) {
    if (mAudioTrack != 0) {
        SL_LOGD("TrackPlayerBase::applyVolumeShaper() from IPlayer");
        VolumeShaper::Status status = mAudioTrack->applyVolumeShaper(configuration, operation);
        if (status < 0) { // a non-negative value is the volume shaper id.
            SL_LOGE("TrackPlayerBase::applyVolumeShaper() failed with status %d", status);
        }
    } else {
        SL_LOGD("TrackPlayerBase::applyVolumeShaper()"
                " no AudioTrack for volume control from IPlayer");
    }
}

status_t TrackPlayerBase::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    return BnPlayer::onTransact(code, data, reply, flags);
}

} // namespace android

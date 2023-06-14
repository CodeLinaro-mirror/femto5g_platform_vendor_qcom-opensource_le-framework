ACLOCAL_AMFLAGS = -I m4

sys_includedir = $(includedir)/system/

if ANDROID_Q_AUDIO_SUPPORT
sys_include_HEADERS = system/audio_android_q/sound_trigger.h \
                      system/audio_android_q/audio.h \
                      system/audio_android_q/audio-base.h \
                      system/audio_android_q/audio-base-utils.h \
                      system/audio_android_q/audio_effect.h \
                      system/audio_android_q/audio_effect-base.h \
                      system/audio_android_q/audio_policy.h
else
sys_include_HEADERS = system/sound_trigger.h \
                      system/audio.h \
                      system/audio_policy.h
endif

audeffect_includedir      = $(includedir)/audio_effects/
audeffect_include_HEADERS = audio_effects/audio_effects_conf.h \
                            audio_effects/effect_aec.h \
                            audio_effects/effect_agc.h \
                            audio_effects/effect_bassboost.h \
                            audio_effects/effect_downmix.h \
                            audio_effects/effect_environmentalreverb.h \
                            audio_effects/effect_equalizer.h \
                            audio_effects/effect_loudnessenhancer.h \
                            audio_effects/effect_ns.h \
                            audio_effects/effect_presetreverb.h \
                            audio_effects/effect_virtualizer.h \
                            audio_effects/effect_visualizer.h

hardware_includedir      = $(includedir)/media/hardware/
hardware_include_HEADERS = hardware/CryptoAPI.h \
                           hardware/MetadataBufferType.h \
                           hardware/HardwareAPI.h \
                           hardware/HDCPAPI.h \
                           hardware/OMXPluginBase.h \
                           hardware/VideoAPI.h

stagefright_includedir      = $(includedir)/media/stagefright/
stagefright_include_HEADERS = stagefright/MediaErrors.h

stagefright_foundation_includedir      = $(includedir)/media/stagefright/foundation/
stagefright_foundation_include_HEADERS = stagefright/foundation/ABase.h

drm_includedir      = $(includedir)/media/drm/
drm_include_HEADERS = drm/DrmAPI.h

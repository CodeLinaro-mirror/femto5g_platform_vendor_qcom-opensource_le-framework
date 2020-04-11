if ANDROID_Q_AUDIO_SUPPORT
h_sources = system/audio_android_q/sound_trigger.h \
            system/audio_android_q/audio.h \
            system/audio_android_q/audio-base.h \
            system/audio_android_q/audio-base-utils.h \
            system/audio_android_q/audio_effect.h \
            system/audio_android_q/audio_effect-base.h \
            system/audio_android_q/audio_policy.h
else
h_sources = system/sound_trigger.h \
            system/audio.h \
            system/audio_policy.h
endif

lib_includedir = $(includedir)/system/
lib_include_HEADERS = $(h_sources)

#pragma once

struct FSoundQualityInfo
{
    /** Holds the number of PCM samples per second. */
    mutable uint32_t SampleRate;

    /** Holds the number of distinct audio channels. */
    mutable uint32_t NumChannels;

    /** Holds the size of sample data in bytes. */
    mutable uint32_t SampleDataSize;

    /** Holds the length of the sound in seconds. */
    mutable float Duration;
};

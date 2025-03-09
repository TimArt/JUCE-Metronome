#pragma once

// Unions internally to allow two names for the same variable.
// - `timeSig.numerator` is same as `timeSig.beatsPerMeasure`
// - `timeSig.denominator` is same as `timeSig.noteTypeForBeat`
struct TimeSignature
{
    // Used for tracking downbeats vs regular beats
    // `union` to allow two names for the same thing
    union
    {
        int numerator, beatsPerMeasure = 4;
    };

    // Type of note that represents a beat
    // 1 = whole, 2 = half, 4 = quarter, 8 = eighth, 16 = sixteenth
    union
    {
        int denominator, noteTypeForBeat = 4;
    };

    bool operator== (const TimeSignature& other) const
    {
        return numerator == other.numerator && denominator == other.denominator;
    }
};
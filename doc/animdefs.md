# ANIMDEFS

A subset of [ZDoom](https://zdoom.org/wiki/ANIMDEFS) `ANIMDEFS` is supported.

## What does work

### flat and textures

- `rand` is not supported
- `allowdecals` is allowed, but does nothing

These are the only supported forms.

List of frames:  
> flat TEST00  
>	pic TEST02 tics 4  
>	pic TEST04 tics 4

Range of frames:  
> flat TEST00 range TEST04 tics 4

(you can repace `flat` by `texture`)

### switches

These are the only supported forms.

Normal switches:  
> switch TESTSW0  
>	on sound switch/test/on  
>	pic TESTSW1 tics 0

Normal switches with different sounds:  
> switch TESTSW0  
>	on sound switch/test/on  
>	pic TESTSW1 tics 0  
>	off sound switch/test/off

Animated switch:  
> switch TESTSW0  
>	on sound switch/test/on  
>	pic TESTSW1 tics 4  
>	pic TESTSW2 tics 4  
>	pic TESTSW3 tics 4  
>	off sound switch/test/off  
>	pic TESTSW2 tics 4  
>	pic TESTSW1 tics 4  

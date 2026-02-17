XML files that can be used to create iccMAX profiles can be found in
the following folders:

## [Calc](Calc)
This folder contains profiles that demonstrate modeling using the
Calculator MultiProcessElement.  The srgbCalcTest profile exercises
all specified calculator operations.

## [Display](Display)
This folder contains profiles that demonstrate spectral modeling of
display profiles allowing for late binding of the observer using
MultiProcessElements that are transformed at startup to colorimetry
for the desired observer.

## [Encoding](Encoding)
This folder contains 3 channel encoding class profiles.  Both "name
only" profiles as well as fully specified profiles are present.

## [Named](Named)
This folder contains named color profiles showcasing features such as
tints, spectral reflectance, and fluorescence (with and with out
sparse notation).

## [PCC](PCC)
This folder contains various profiles that can be used to define Profile
Connection Conditions (PCC).  All profiles are abstract profiles that
perform no operation to PCS values.  However, all profiles contain fully
defined PCC tags that provide information that can be used to define
rendering for various observers and illuminants.  Profiles that utilize
both absoulute colorimetry as well as Material Adjusted colorimetry
are present.

## [SpecRef](SpecRef)
This folder contains various profiles that convert data to/from/betweteen
a spectral reflectance PCS.  The argbRef (AdobeRGB) and srgbRef (sRGB)
convert RGB values to/from spectral reflectance.  RefDecC, RefDecH, and
RefIncW are abstract spectral reflectance profiles that modify "chroma",
"hue", and "lightness" of spectral reflectance values in a spectral
reflectance PCS.  
The argbRef, srgbRef, RefDecC, RefDecH, RefIncW profiles all estimate
and/or manipulate spectral reflectance using Wpt based spectral estimation
(see chapter 7 of http://scholarworks.rit.edu/theses/8789/ ).  
Additionally, examples of 6 channel abridged spectral encoding is provided.

## Note:
The CreateAllProfiles.bat/.sh files use `iccFromXML` to create ICC profiles
from each of the XML files in these folders. Add the PATH to your local devenv.

For [Release Downloads](https://github.com/InternationalColorConsortium/iccDEV/releases)
run the included PATH Scripts:

- Windows run `path.bat` then `CreateAllProfiles.bat`
- Unix run `./path.sh` then `./CreateAllProfiles.sh`

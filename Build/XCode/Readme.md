# Xcode Project Create & Build

**Quick Start** `brew install iccdev`

The instructions below will create a new Xcode Project for iccDEV and compile the Tools.

## Create and Build the Project

```
brew install nlohmann-json libxml2 wxwidgets libtiff libpng jpeg
cd Build/Xcode && cmake -G "Xcode" ../Cmake/ && xcodebuild
```

## Open the Project

`open RefIccMAX.xcodeproj`

## Use the Tools

```
./Tools/IccFromXml/Release/iccFromXml
IccFromXml built with IccProfLib Version 2.2.5, IccLibXML Version 2.2.5

Usage: IccFromXml xml_file saved_profile_file {-noid -v{=[relax_ng_schema_file - optional]}}
```


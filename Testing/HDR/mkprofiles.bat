@echo off
iccFromXml BT2100HlgFullScene.xml BT2100HlgFullScene.icc
iccFromXml BT2100HlgNarrowScene.xml BT2100HlgNarrowScene.icc
iccFromXml BT2100HlgFullDisplay.xml BT2100HlgFullDisplay.icc
iccFromXml BT2100HlgNarrowDisplay.xml BT2100HlgNarrowDisplay.icc

iccFromXml BT2100PQFullScene.xml BT2100PQFullScene.icc
iccFromXml BT2100PQNarrowScene.xml BT2100PQNarrowScene.icc
iccFromXml BT2100PQFullDisplay.xml BT2100PQFullDisplay.icc
iccFromXml BT2100PQNarrowDisplay.xml BT2100PQNarrowDisplay.icc

iccFromXml BT2100HlgSceneToDisplayLink.xml BT2100HlgSceneToDisplayLink.icc
iccFromXml BT2100PQSceneToDisplayLink.xml BT2100PQSceneToDisplayLink.icc

REM headroomAdaptiveGainCurveTag ('HAGC') coverage. The three positives cover the
REM three shapes the encoding takes: the full layout, the two sharing flags that
REM delete fields from every alternate after the first, and the raw-byte authoring
REM path. HagcInvalidXOrder is a negative -- see Testing/expected-invalid-fromxml.tsv.
iccFromXml HagcDisplay.xml HagcDisplay.icc
iccFromXml HagcCommonParams.xml HagcCommonParams.icc
iccFromXml HagcHexData.xml HagcHexData.icc
iccFromXml HagcInvalidXOrder.xml HagcInvalidXOrder.icc

REM Clause 8.10 HDR Profile coverage: a conforming profile carrying HDR Image and
REM HDR Display metadata but no tone-mapping descriptor, and two negatives for the
REM TransferCharacteristics restriction and the AToBx/BToAx pairing rule.
iccFromXml HdrCicpUnspecified.xml HdrCicpUnspecified.icc
iccFromXml HdrDisplayMetadata.xml HdrDisplayMetadata.icc
iccFromXml HdrInvalidTransfer.xml HdrInvalidTransfer.icc
iccFromXml HdrMissingBToA0.xml HdrMissingBToA0.icc

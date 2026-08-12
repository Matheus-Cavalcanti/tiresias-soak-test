### Current Message Ownership

| Message/channel | What it represents | Current producer | Architectural consumer/owner |
|---|---|---|---|
| `le_audio_msg` / `le_audio_chan` | BAP stream configuration and lifecycle callbacks | Broadcast Sink, Broadcast Source, or unicast implementation selected by configuration | Audio Streaming subsystem |
| `bt_mgmt_msg` / `bt_mgmt_chan` | Mixed ACL, advertising, PA synchronization, and BASS events | Bluetooth Management modules | Currently split between Control Link and Audio Streaming |
| `sdu_ref_msg` / `sdu_ref_chan` | Transmit timing-reference information | LE Audio transmit path | Audio data plane |
| `volume_msg` / `volume_chan` | Standard VCS volume and mute requests | Bluetooth Rendering and Capture adapter | Future Codec Controller or dedicated volume owner |
| `content_control_msg` / `cont_media_chan` | Standard media start/stop requests | Bluetooth Content Control adapter | Future device/media policy owner |
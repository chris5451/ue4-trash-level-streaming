# ue4-trash-level-streaming
A plugin for loading and unloading "trash" objects in multiplayer games independent of host player

# UE4 Versions

This plugin is testet in 4.25

# Why

When the host of a listen server game loads or unloads a streaming level all clients follow that change. This is not ideal for some usecases. If you have a lot of "trash" objects that clutter your scene and don´t affect gameplay it would be ideal to only load these streaming levels locally

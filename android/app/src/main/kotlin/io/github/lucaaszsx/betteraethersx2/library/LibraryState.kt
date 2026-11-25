package io.github.lucaaszsx.betteraethersx2.library

// What the library shows for the game folder.
sealed interface LibraryState {
    data object NoFolder : LibraryState

    data class Scanning(
        val found: Int,
    ) : LibraryState

    data class Loaded(
        val games: List<Game>,
    ) : LibraryState

    // The folder is set, but Android took the permission back.
    data object AccessLost : LibraryState
}

# Code signing (SignPath)

The workflow (`.github/workflows/build.yml`) can send the Windows setup it builds to
[SignPath](https://signpath.io) for signing. It isn't active: the SignPath Foundation's
free program asks for a project that is already widely known, so the application can
be made again later; a paid SignPath subscription would work too.

To turn it on:

1. In SignPath, the project's slug is `wasdmod`, with a signing policy `release-signing`
   (manual approval) and the artifact configuration in `artifact-configuration.xml`.
2. Connect the repository to SignPath as a trusted build system (GitHub Actions).
3. In GitHub, Settings → Secrets and variables → Actions:
   - variable `SIGNPATH_ORGANIZATION_ID`: the organization ID from SignPath;
   - secret `SIGNPATH_API_TOKEN`: an API token of a SignPath user that can submit
     signing requests for the project.

Then publish a release on GitHub without attaching files: the workflow builds them,
sends the setup to SignPath, waits (up to an hour) for the request to be approved
there, and attaches the signed setup, the Mac app and the zip to the release.

Until the variable is set, releases get the same files unsigned.

# Code signing (SignPath Foundation)

The Windows setup is signed by the [SignPath Foundation](https://signpath.org), built
from this repository by GitHub Actions (`.github/workflows/build.yml`).

Once SignPath has approved the project:

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

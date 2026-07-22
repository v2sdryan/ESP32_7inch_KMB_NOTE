# Google Tasks bridge setup

The ESP32 does not store a Google OAuth client secret or refresh token. Google
does not list Tasks among the scopes supported by its limited-input device
authorization flow, so the dashboard calls a small Google Apps Script web app.
Apps Script performs OAuth under the owner's Google account and returns only
the due tasks needed by the dashboard.

## What the owner must create

1. Create a standalone Apps Script project and paste the repository file
   google-apps-script/Code.gs into its editor.
2. In Services, add the advanced Google Tasks API service.
3. In Project settings, link a standard Google Cloud project. In that Cloud
   project, enable Google Tasks API. No API key is required.
4. Add a Script Property named DEVICE_TOKEN with a randomly generated value of
   32 or more characters. Treat this value like a password.
5. Deploy as a web app, executing as yourself. Select the narrowest access
   option that still permits the ESP32's unauthenticated HTTPS POST.
6. Copy the deployment URL ending in /exec.

Enter the deployment URL and DEVICE_TOKEN through the dashboard's private
configuration portal, enable Google Tasks, and leave the list ID as @default
unless a different list is required. Never commit the token or send it in a
public chat. The ESP32 validates Google's TLS certificate using the curated
root bundle stored in LittleFS.

Google recommends refreshing its curated trust bundle at least twice yearly.
Replace data/google-roots.pem with the latest https://pki.goog/roots.pem before
a maintenance release, then rebuild and upload LittleFS.

/*
 * ============================================================
 *  Project Title  : MedTrack – Hospital Patient Registration
 *                   & Follow-up Tracker
 *  DA1 Question   : 8
 *  Course         : C Programming
 *  Faculty        : Dr. Dinakaran M
 *
 * ============================================================
 *  C CONCEPTS USED
 * ============================================================
 *  1. struct            – Patient, Visit  (typedef structs)
 *  2. File Handling     – fopen / fread / fwrite / fclose
 *                         Binary .dat files (patients.dat,
 *                         visits.dat)
 *  3. User-defined      – addPatient, addVisit, editPatient,
 *     functions           deletePatient, searchPatient,
 *                         getAllPatients, getAllVisits,
 *                         getVisitHistory, getFrequentVisitors,
 *                         getStats, validatePhone, validateAge,
 *                         findPatientIndex, countVisits,
 *                         getLastVisitDate, escapeJsonStr,
 *                         buildPatientJson, buildVisitJson,
 *                         toLowerStr  (17 functions total)
 *  4. Menu-driven       – main() with do-while + switch/case
 *     program
 *  5. Input Validation  – validatePhone() – exactly 10 digits
 *                         validateAge()   – 0 to 150
 *                         duplicate ID check
 *  6. String operations – strcmp, strcpy, strncpy, strstr,
 *                         strlen, snprintf
 *  7. Arrays & Pointers – patients[], visits[], char* return
 *
 * ============================================================
 *  HOW TO COMPILE  (console / local testing)
 * ============================================================
 *  gcc main.c -o medtrack
 *  ./medtrack
 *
 * ============================================================
 *  HOW TO COMPILE  → WebAssembly  (for index.html browser UI)
 * ============================================================
 *  emcc main.c -o index.js \
 *    -s EXPORTED_FUNCTIONS='["_main","_addPatient","_addVisit",\
 *       "_editPatient","_deletePatient","_searchPatient",\
 *       "_getAllPatients","_getAllVisits","_getVisitHistory",\
 *       "_getFrequentVisitors","_getStats"]' \
 *    -s EXPORTED_RUNTIME_METHODS='["ccall"]' \
 *    -s ALLOW_MEMORY_GROWTH=1 \
 *    -s NO_EXIT_RUNTIME=1
 *
 *  Then place index.html, index.js, index.wasm in the same
 *  folder and open index.html via a local server or GitHub Pages.
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Emscripten: expose functions to JavaScript ── */
#ifdef __EMSCRIPTEN__
  #include <emscripten.h>
  #define EXPORT EMSCRIPTEN_KEEPALIVE
#else
  #define EXPORT
#endif

/* ============================================================
   CONSTANTS
============================================================ */
#define MAX_PATIENTS       500
#define MAX_VISITS        2000
#define MAX_ID              12   /* "P001\0"                            */
#define MAX_NAME            80   /* Full patient name                   */
#define MAX_PHONE           12   /* 10 digits + null                    */
#define MAX_GENDER          16   /* "Male","Female","Other"             */
#define MAX_BLOOD            6   /* "AB+\0"                             */
#define MAX_ADDR           120   /* Home address (optional)             */
#define MAX_DATE            12   /* "YYYY-MM-DD\0"                      */
#define MAX_DIAG            80   /* Diagnosis text                      */
#define MAX_NOTES          400   /* Prescription / treatment notes      */
#define MAX_DOCTOR          60   /* Attending doctor name               */
#define MAX_SEV             14   /* "Emergency\0"                       */

#define FREQUENT_THRESHOLD   3   /* visits needed to be "frequent"      */

#define PATIENT_FILE  "patients.dat"
#define VISIT_FILE    "visits.dat"

/* ============================================================
   STRUCTS
============================================================ */

/*
 * Patient – one registered patient record.
 *
 *  id       : unique identifier assigned by staff, e.g. "P001"
 *  name     : full legal name
 *  age      : validated 0–150
 *  phone    : exactly 10 digits
 *  gender   : "Male" / "Female" / "Other"
 *  blood    : "A+" / "B-" / "O+" / "AB-" etc.
 *  address  : home address, optional
 *  active   : 1 = live record, 0 = soft-deleted
 */
typedef struct {
    char id     [MAX_ID];
    char name   [MAX_NAME];
    int  age;
    char phone  [MAX_PHONE];
    char gender [MAX_GENDER];
    char blood  [MAX_BLOOD];
    char address[MAX_ADDR];
    int  active;
} Patient;

/*
 * Visit – one clinical visit linked to a Patient.
 *
 *  patientId : foreign key → Patient.id
 *  date      : ISO date string "YYYY-MM-DD"
 *  diagnosis : primary diagnosis text
 *  notes     : prescription / treatment notes
 *  doctor    : attending physician name
 *  severity  : "Routine" / "Moderate" / "Severe" / "Emergency"
 *  active    : 1 = live, 0 = soft-deleted
 */
typedef struct {
    char patientId[MAX_ID];
    char date     [MAX_DATE];
    char diagnosis[MAX_DIAG];
    char notes    [MAX_NOTES];
    char doctor   [MAX_DOCTOR];
    char severity [MAX_SEV];
    int  active;
} Visit;

/* ── In-memory data stores ── */
static Patient patients[MAX_PATIENTS];
static Visit   visits  [MAX_VISITS];
static int     patientCount = 0;
static int     visitCount   = 0;

/* ── Shared output buffer for all JSON return values (256 KB) ── */
static char outBuf[1024 * 256];

/* ============================================================
   FORWARD DECLARATIONS
============================================================ */
void        loadFromFiles    (void);
void        saveToFiles      (void);
int         findPatientIndex (const char *id);
int         countVisits      (const char *patientId);
const char *getLastVisitDate (const char *patientId);
int         validatePhone    (const char *phone);
int         validateAge      (int age);
void        toLowerStr       (const char *src, char *dst, int maxLen);
void        escapeJsonStr    (const char *src, char *dst, int maxLen);
void        buildPatientJson (int idx, char *buf, int bufSize);
void        buildVisitJson   (int idx, char *buf, int bufSize);

/* ============================================================
   FILE HANDLING
   Binary format stored in .dat files:
     [count : int][record_0][record_1]...[record_n-1]
============================================================ */

/*
 * loadFromFiles – reads both .dat files into memory at startup.
 * Silently continues if files are missing (first run is fine).
 */
void loadFromFiles(void)
{
    FILE *fp;

    /* Load patients */
    fp = fopen(PATIENT_FILE, "rb");
    if (fp) {
        fread(&patientCount, sizeof(int), 1, fp);
        if (patientCount < 0 || patientCount > MAX_PATIENTS)
            patientCount = 0;
        fread(patients, sizeof(Patient), patientCount, fp);
        fclose(fp);
    }

    /* Load visits */
    fp = fopen(VISIT_FILE, "rb");
    if (fp) {
        fread(&visitCount, sizeof(int), 1, fp);
        if (visitCount < 0 || visitCount > MAX_VISITS)
            visitCount = 0;
        fread(visits, sizeof(Visit), visitCount, fp);
        fclose(fp);
    }
}

/*
 * saveToFiles – writes the current in-memory arrays to .dat files.
 * Called after every add / edit / delete operation.
 */
void saveToFiles(void)
{
    FILE *fp;

    fp = fopen(PATIENT_FILE, "wb");
    if (fp) {
        fwrite(&patientCount, sizeof(int),     1,            fp);
        fwrite(patients,      sizeof(Patient), patientCount, fp);
        fclose(fp);
    }

    fp = fopen(VISIT_FILE, "wb");
    if (fp) {
        fwrite(&visitCount, sizeof(int),   1,          fp);
        fwrite(visits,      sizeof(Visit), visitCount, fp);
        fclose(fp);
    }
}

/* ============================================================
   UTILITY / HELPER FUNCTIONS
============================================================ */

/*
 * findPatientIndex – linear search for an active patient by ID.
 * Returns the array index (0-based), or -1 if not found.
 */
int findPatientIndex(const char *id)
{
    int i;
    for (i = 0; i < patientCount; i++) {
        if (patients[i].active && strcmp(patients[i].id, id) == 0)
            return i;
    }
    return -1;
}

/*
 * countVisits – counts the active visit records for one patient.
 */
int countVisits(const char *patientId)
{
    int i, count = 0;
    for (i = 0; i < visitCount; i++) {
        if (visits[i].active &&
            strcmp(visits[i].patientId, patientId) == 0)
            count++;
    }
    return count;
}

/*
 * getLastVisitDate – returns the most recent visit date for a
 * patient as an ISO string. ISO dates sort correctly as plain
 * strings, so we use strcmp for comparison.
 * Returns "" if the patient has no visits.
 */
const char *getLastVisitDate(const char *patientId)
{
    static char best[MAX_DATE];
    int i;
    best[0] = '\0';
    for (i = 0; i < visitCount; i++) {
        if (visits[i].active &&
            strcmp(visits[i].patientId, patientId) == 0) {
            if (strcmp(visits[i].date, best) > 0)
                strncpy(best, visits[i].date, MAX_DATE - 1);
        }
    }
    return best;
}

/*
 * validatePhone – returns 1 if the string is exactly 10 decimal
 * digit characters, 0 otherwise.
 * Mirrors the JavaScript regex  /^\d{10}$/
 */
int validatePhone(const char *phone)
{
    int i, len;
    if (!phone) return 0;
    len = (int)strlen(phone);
    if (len != 10) return 0;
    for (i = 0; i < len; i++) {
        if (!isdigit((unsigned char)phone[i])) return 0;
    }
    return 1;
}

/*
 * validateAge – returns 1 if age is in the range [0, 150].
 */
int validateAge(int age)
{
    return (age >= 0 && age <= 150);
}

/*
 * toLowerStr – copies src into dst with every character
 * lowercased. Used for case-insensitive search, matching the
 * JavaScript  str.toLowerCase()  calls in index.html.
 */
void toLowerStr(const char *src, char *dst, int maxLen)
{
    int i = 0;
    while (src[i] && i < maxLen - 1) {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

/*
 * escapeJsonStr – escapes  "  and  \  so that the string can be
 * safely embedded as a JSON value without breaking the structure.
 */
void escapeJsonStr(const char *src, char *dst, int maxLen)
{
    int si = 0, di = 0;
    while (src[si] && di < maxLen - 2) {
        if (src[si] == '"' || src[si] == '\\')
            dst[di++] = '\\';
        dst[di++] = src[si++];
    }
    dst[di] = '\0';
}

/*
 * buildPatientJson – serialises one Patient record into a JSON
 * object string.  Field names must match the JavaScript
 * withMeta() object consumed by index.html.
 */
void buildPatientJson(int idx, char *buf, int bufSize)
{
    char eName[MAX_NAME * 2];
    char eAddr[MAX_ADDR * 2];
    int  vc = countVisits(patients[idx].id);

    escapeJsonStr(patients[idx].name,    eName, sizeof(eName));
    escapeJsonStr(patients[idx].address, eAddr, sizeof(eAddr));

    snprintf(buf, bufSize,
        "{"
        "\"id\":\"%s\","
        "\"name\":\"%s\","
        "\"age\":%d,"
        "\"phone\":\"%s\","
        "\"gender\":\"%s\","
        "\"blood\":\"%s\","
        "\"address\":\"%s\","
        "\"visitCount\":%d,"
        "\"lastVisit\":\"%s\","
        "\"isFrequent\":%s"
        "}",
        patients[idx].id,
        eName,
        patients[idx].age,
        patients[idx].phone,
        patients[idx].gender,
        patients[idx].blood,
        eAddr,
        vc,
        getLastVisitDate(patients[idx].id),
        vc >= FREQUENT_THRESHOLD ? "true" : "false"
    );
}

/*
 * buildVisitJson – serialises one Visit record into a JSON
 * object string.
 */
void buildVisitJson(int idx, char *buf, int bufSize)
{
    char eDiag[MAX_DIAG   * 2];
    char eNote[MAX_NOTES  * 2];
    char eDoc [MAX_DOCTOR * 2];

    escapeJsonStr(visits[idx].diagnosis, eDiag, sizeof(eDiag));
    escapeJsonStr(visits[idx].notes,     eNote, sizeof(eNote));
    escapeJsonStr(visits[idx].doctor,    eDoc,  sizeof(eDoc));

    snprintf(buf, bufSize,
        "{"
        "\"patientId\":\"%s\","
        "\"date\":\"%s\","
        "\"diagnosis\":\"%s\","
        "\"notes\":\"%s\","
        "\"doctor\":\"%s\","
        "\"severity\":\"%s\""
        "}",
        visits[idx].patientId,
        visits[idx].date,
        eDiag,
        eNote,
        eDoc,
        visits[idx].severity
    );
}

/* ============================================================
   EXPORTED FUNCTIONS
   Called by JavaScript via  Module.ccall()  when compiled with
   Emscripten.  Each function returns a pointer to outBuf which
   holds a JSON string — either an object or an array.
============================================================ */

/* ----------------------------------------------------------
   addPatient
   Registers a new patient after full input validation.
   Returns: { "ok": true/false, "msg": "..." }
---------------------------------------------------------- */
EXPORT const char *addPatient(const char *id,
                               const char *name,
                               int         age,
                               const char *phone,
                               const char *gender,
                               const char *blood,
                               const char *address)
{
    int i;

    /* Validate required fields */
    if (!id || strlen(id) == 0) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Patient ID is required\"}");
        return outBuf;
    }
    if (!name || strlen(name) == 0) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Patient name is required\"}");
        return outBuf;
    }
    if (!validateAge(age)) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Invalid age (must be 0-150)\"}");
        return outBuf;
    }
    if (!validatePhone(phone)) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Phone must be exactly 10 digits\"}");
        return outBuf;
    }

    /* Duplicate ID check */
    if (findPatientIndex(id) >= 0) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Patient ID '%s' already exists\"}", id);
        return outBuf;
    }

    /* Capacity guard */
    if (patientCount >= MAX_PATIENTS) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Maximum patient capacity reached\"}");
        return outBuf;
    }

    /* Create record */
    i = patientCount++;
    memset(&patients[i], 0, sizeof(Patient));
    strncpy(patients[i].id,      id,                           MAX_ID     - 1);
    strncpy(patients[i].name,    name,                         MAX_NAME   - 1);
    patients[i].age = age;
    strncpy(patients[i].phone,   phone,                        MAX_PHONE  - 1);
    strncpy(patients[i].gender,  gender  ? gender  : "Not Specified",
                                                               MAX_GENDER - 1);
    strncpy(patients[i].blood,   blood   ? blood   : "Unknown",MAX_BLOOD  - 1);
    strncpy(patients[i].address, address ? address : "",       MAX_ADDR   - 1);
    patients[i].active = 1;

    saveToFiles();

    snprintf(outBuf, sizeof(outBuf),
             "{\"ok\":true,\"msg\":\"Patient '%s' registered successfully\"}", name);
    return outBuf;
}

/* ----------------------------------------------------------
   addVisit
   Records a clinical visit for an existing patient.
   Returns: { "ok": true/false, "msg": "..." }
---------------------------------------------------------- */
EXPORT const char *addVisit(const char *patientId,
                             const char *date,
                             const char *diagnosis,
                             const char *notes,
                             const char *doctor,
                             const char *severity)
{
    int i;

    if (!patientId || strlen(patientId) == 0) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Patient ID is required\"}");
        return outBuf;
    }
    if (!date || strlen(date) == 0) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Visit date is required\"}");
        return outBuf;
    }
    if (!diagnosis || strlen(diagnosis) == 0) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Diagnosis is required\"}");
        return outBuf;
    }

    /* Patient must already be registered */
    if (findPatientIndex(patientId) < 0) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Patient '%s' not found. "
                 "Register patient first.\"}", patientId);
        return outBuf;
    }

    if (visitCount >= MAX_VISITS) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Maximum visit capacity reached\"}");
        return outBuf;
    }

    /* Create record */
    i = visitCount++;
    memset(&visits[i], 0, sizeof(Visit));
    strncpy(visits[i].patientId, patientId,                     MAX_ID     - 1);
    strncpy(visits[i].date,      date,                          MAX_DATE   - 1);
    strncpy(visits[i].diagnosis, diagnosis,                     MAX_DIAG   - 1);
    strncpy(visits[i].notes,     notes    ? notes    : "",      MAX_NOTES  - 1);
    strncpy(visits[i].doctor,    doctor   ? doctor   : "Unknown",
                                                                MAX_DOCTOR - 1);
    strncpy(visits[i].severity,  severity ? severity : "Routine",
                                                                MAX_SEV    - 1);
    visits[i].active = 1;

    saveToFiles();

    snprintf(outBuf, sizeof(outBuf),
             "{\"ok\":true,\"msg\":\"Visit recorded for patient '%s'\"}", patientId);
    return outBuf;
}

/* ----------------------------------------------------------
   editPatient
   Updates mutable fields of an existing patient.
   Patient ID itself cannot be changed.
   Returns: { "ok": true/false, "msg": "..." }
---------------------------------------------------------- */
EXPORT const char *editPatient(const char *id,
                                const char *name,
                                int         age,
                                const char *phone,
                                const char *gender,
                                const char *blood,
                                const char *address)
{
    int idx = findPatientIndex(id);
    if (idx < 0) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Patient '%s' not found\"}", id);
        return outBuf;
    }
    if (!validateAge(age)) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Invalid age (must be 0-150)\"}");
        return outBuf;
    }
    if (!validatePhone(phone)) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Phone must be exactly 10 digits\"}");
        return outBuf;
    }

    strncpy(patients[idx].name,    name,                    MAX_NAME   - 1);
    patients[idx].age = age;
    strncpy(patients[idx].phone,   phone,                   MAX_PHONE  - 1);
    strncpy(patients[idx].gender,  gender  ? gender  : "Not Specified",
                                                             MAX_GENDER - 1);
    strncpy(patients[idx].blood,   blood   ? blood   : "Unknown",
                                                             MAX_BLOOD  - 1);
    strncpy(patients[idx].address, address ? address : "", MAX_ADDR   - 1);

    saveToFiles();

    snprintf(outBuf, sizeof(outBuf),
             "{\"ok\":true,\"msg\":\"Patient '%s' updated successfully\"}", name);
    return outBuf;
}

/* ----------------------------------------------------------
   deletePatient
   Soft-deletes a patient and all their visit records.
   (Sets active = 0 instead of removing from the array,
    preserving file integrity.)
   Returns: { "ok": true/false, "msg": "..." }
---------------------------------------------------------- */
EXPORT const char *deletePatient(const char *id)
{
    int i;
    char savedName[MAX_NAME];

    int idx = findPatientIndex(id);
    if (idx < 0) {
        snprintf(outBuf, sizeof(outBuf),
                 "{\"ok\":false,\"msg\":\"Patient '%s' not found\"}", id);
        return outBuf;
    }

    strncpy(savedName, patients[idx].name, MAX_NAME - 1);

    patients[idx].active = 0;                   /* soft-delete patient */

    for (i = 0; i < visitCount; i++) {          /* soft-delete visits  */
        if (strcmp(visits[i].patientId, id) == 0)
            visits[i].active = 0;
    }

    saveToFiles();

    snprintf(outBuf, sizeof(outBuf),
             "{\"ok\":true,\"msg\":\"Patient '%s' and their visits "
             "have been removed\"}", savedName);
    return outBuf;
}

/* ----------------------------------------------------------
   searchPatient
   Case-insensitive partial match against ID, name, or phone.
   Returns: JSON array of matching Patient objects.
---------------------------------------------------------- */
EXPORT const char *searchPatient(const char *query)
{
    int i, found = 0, written = 0;
    char lq [MAX_NAME];
    char ln [MAX_NAME];
    char lid[MAX_ID];
    char itemBuf[2048];

    toLowerStr(query, lq, sizeof(lq));

    written = snprintf(outBuf, sizeof(outBuf), "[");

    for (i = 0; i < patientCount; i++) {
        if (!patients[i].active) continue;

        toLowerStr(patients[i].name, ln,  sizeof(ln));
        toLowerStr(patients[i].id,   lid, sizeof(lid));

        if (strstr(ln,  lq) ||
            strstr(lid, lq) ||
            strstr(patients[i].phone, query))
        {
            buildPatientJson(i, itemBuf, sizeof(itemBuf));
            if (found > 0)
                written += snprintf(outBuf + written,
                                    sizeof(outBuf) - written, ",");
            written += snprintf(outBuf + written,
                                sizeof(outBuf) - written,
                                "%s", itemBuf);
            found++;
        }
    }

    snprintf(outBuf + written, sizeof(outBuf) - written, "]");
    return outBuf;
}

/* ----------------------------------------------------------
   getAllPatients
   Returns a JSON array of every active patient.
---------------------------------------------------------- */
EXPORT const char *getAllPatients(void)
{
    int i, count = 0, written = 0;
    char itemBuf[2048];

    written = snprintf(outBuf, sizeof(outBuf), "[");

    for (i = 0; i < patientCount; i++) {
        if (!patients[i].active) continue;
        buildPatientJson(i, itemBuf, sizeof(itemBuf));
        if (count > 0)
            written += snprintf(outBuf + written,
                                sizeof(outBuf) - written, ",");
        written += snprintf(outBuf + written,
                            sizeof(outBuf) - written, "%s", itemBuf);
        count++;
    }

    snprintf(outBuf + written, sizeof(outBuf) - written, "]");
    return outBuf;
}

/* ----------------------------------------------------------
   getAllVisits
   Returns a JSON array of all active visits, sorted
   newest-first by date (ISO strings sort correctly).
---------------------------------------------------------- */
EXPORT const char *getAllVisits(void)
{
    int sorted[MAX_VISITS];
    int n = 0, i, j, tmp;
    int count = 0, written = 0;
    char itemBuf[2048];

    for (i = 0; i < visitCount; i++)
        if (visits[i].active) sorted[n++] = i;

    /* Insertion sort – descending by date string */
    for (i = 1; i < n; i++) {
        tmp = sorted[i]; j = i - 1;
        while (j >= 0 &&
               strcmp(visits[sorted[j]].date, visits[tmp].date) < 0) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = tmp;
    }

    written = snprintf(outBuf, sizeof(outBuf), "[");
    for (i = 0; i < n; i++) {
        buildVisitJson(sorted[i], itemBuf, sizeof(itemBuf));
        if (count > 0)
            written += snprintf(outBuf + written,
                                sizeof(outBuf) - written, ",");
        written += snprintf(outBuf + written,
                            sizeof(outBuf) - written, "%s", itemBuf);
        count++;
    }
    snprintf(outBuf + written, sizeof(outBuf) - written, "]");
    return outBuf;
}

/* ----------------------------------------------------------
   getVisitHistory
   Returns a JSON array of all active visits for one patient,
   sorted newest-first.
---------------------------------------------------------- */
EXPORT const char *getVisitHistory(const char *patientId)
{
    int sorted[MAX_VISITS];
    int n = 0, i, j, tmp;
    int count = 0, written = 0;
    char itemBuf[2048];

    for (i = 0; i < visitCount; i++) {
        if (visits[i].active &&
            strcmp(visits[i].patientId, patientId) == 0)
            sorted[n++] = i;
    }

    /* Insertion sort – descending by date string */
    for (i = 1; i < n; i++) {
        tmp = sorted[i]; j = i - 1;
        while (j >= 0 &&
               strcmp(visits[sorted[j]].date, visits[tmp].date) < 0) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = tmp;
    }

    written = snprintf(outBuf, sizeof(outBuf), "[");
    for (i = 0; i < n; i++) {
        buildVisitJson(sorted[i], itemBuf, sizeof(itemBuf));
        if (count > 0)
            written += snprintf(outBuf + written,
                                sizeof(outBuf) - written, ",");
        written += snprintf(outBuf + written,
                            sizeof(outBuf) - written, "%s", itemBuf);
        count++;
    }
    snprintf(outBuf + written, sizeof(outBuf) - written, "]");
    return outBuf;
}

/* ----------------------------------------------------------
   getFrequentVisitors
   Returns a JSON array of patients with >= FREQUENT_THRESHOLD
   visits, sorted by visit count descending (bubble sort).
---------------------------------------------------------- */
EXPORT const char *getFrequentVisitors(void)
{
    int indices[MAX_PATIENTS];
    int vcounts[MAX_PATIENTS];
    int n = 0, i, j, ti, tv;
    int count = 0, written = 0;
    char itemBuf[2048];

    for (i = 0; i < patientCount; i++) {
        if (!patients[i].active) continue;
        int vc = countVisits(patients[i].id);
        if (vc >= FREQUENT_THRESHOLD) {
            indices[n] = i;
            vcounts[n] = vc;
            n++;
        }
    }

    /* Bubble sort – descending by visit count */
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - i - 1; j++) {
            if (vcounts[j] < vcounts[j + 1]) {
                ti = indices[j];  tv = vcounts[j];
                indices[j]   = indices[j + 1]; vcounts[j]   = vcounts[j + 1];
                indices[j+1] = ti;             vcounts[j+1] = tv;
            }
        }
    }

    written = snprintf(outBuf, sizeof(outBuf), "[");
    for (i = 0; i < n; i++) {
        buildPatientJson(indices[i], itemBuf, sizeof(itemBuf));
        if (count > 0)
            written += snprintf(outBuf + written,
                                sizeof(outBuf) - written, ",");
        written += snprintf(outBuf + written,
                            sizeof(outBuf) - written, "%s", itemBuf);
        count++;
    }
    snprintf(outBuf + written, sizeof(outBuf) - written, "]");
    return outBuf;
}

/* ----------------------------------------------------------
   getStats
   Returns aggregate statistics as a single JSON object.
   Used by the dashboard and live stat sidebar in index.html.
---------------------------------------------------------- */
EXPORT const char *getStats(void)
{
    int i;
    int totalPts = 0, totalVis = 0, freqCount = 0;
    int sevRout  = 0, sevMod  = 0;
    int sevSev   = 0, sevEmerg= 0;

    for (i = 0; i < patientCount; i++) {
        if (!patients[i].active) continue;
        totalPts++;
        if (countVisits(patients[i].id) >= FREQUENT_THRESHOLD)
            freqCount++;
    }
    for (i = 0; i < visitCount; i++) {
        if (!visits[i].active) continue;
        totalVis++;
        if      (strcmp(visits[i].severity, "Routine")   == 0) sevRout++;
        else if (strcmp(visits[i].severity, "Moderate")  == 0) sevMod++;
        else if (strcmp(visits[i].severity, "Severe")    == 0) sevSev++;
        else if (strcmp(visits[i].severity, "Emergency") == 0) sevEmerg++;
    }

    snprintf(outBuf, sizeof(outBuf),
        "{"
        "\"totalPatients\":%d,"
        "\"totalVisits\":%d,"
        "\"frequentVisitors\":%d,"
        "\"severityRoutine\":%d,"
        "\"severityModerate\":%d,"
        "\"severitySevere\":%d,"
        "\"severityEmergency\":%d"
        "}",
        totalPts, totalVis, freqCount,
        sevRout, sevMod, sevSev, sevEmerg);
    return outBuf;
}

/* ============================================================
   CONSOLE MAIN
   Active ONLY when compiled without Emscripten (gcc / local).
   Full menu-driven program using do-while + switch/case.
============================================================ */
#ifndef __EMSCRIPTEN__

static void dline(void) {
    printf("  ════════════════════════════════════════════════\n");
}
static void line(void) {
    printf("  ────────────────────────────────────────────────\n");
}

/* ── Option 1: Register patient ── */
static void menuRegisterPatient(void)
{
    char id[MAX_ID], name[MAX_NAME], phone[MAX_PHONE];
    char gender[MAX_GENDER], blood[MAX_BLOOD], address[MAX_ADDR];
    int  age;

    dline();
    printf("  [ 1 ]  REGISTER NEW PATIENT\n");
    dline();
    printf("  Patient ID        (e.g. P001)  : "); scanf("%11s",       id);
    printf("  Full Name                      : "); scanf(" %79[^\n]",  name);
    printf("  Age               (0-150)      : "); scanf("%d",         &age);
    printf("  Phone             (10 digits)  : "); scanf("%11s",       phone);
    printf("  Gender (Male/Female/Other)     : "); scanf("%15s",       gender);
    printf("  Blood Group  (A+/B-/O+/AB+...) : "); scanf("%5s",       blood);
    printf("  Address       (optional)       : "); scanf(" %119[^\n]", address);

    printf("\n  Result: %s\n",
           addPatient(id, name, age, phone, gender, blood, address));
}

/* ── Option 2: Add visit ── */
static void menuAddVisit(void)
{
    char pid[MAX_ID], date[MAX_DATE], diag[MAX_DIAG];
    char notes[MAX_NOTES], doc[MAX_DOCTOR], sev[MAX_SEV];

    dline();
    printf("  [ 2 ]  ADD VISIT RECORD\n");
    dline();
    printf("  Patient ID                       : "); scanf("%11s",       pid);
    printf("  Date          (YYYY-MM-DD)       : "); scanf("%11s",       date);
    printf("  Diagnosis                        : "); scanf(" %79[^\n]",  diag);
    printf("  Prescription / Notes (optional)  : "); scanf(" %399[^\n]", notes);
    printf("  Doctor Name                      : "); scanf(" %59[^\n]",  doc);
    printf("  Severity\n");
    printf("    (Routine/Moderate/Severe/Emergency) : "); scanf("%13s",   sev);

    printf("\n  Result: %s\n",
           addVisit(pid, date, diag, notes, doc, sev));
}

/* ── Option 3: Search patient ── */
static void menuSearch(void)
{
    char q[MAX_NAME];
    dline();
    printf("  [ 3 ]  SEARCH PATIENT\n");
    dline();
    printf("  Enter ID, Name or Phone  : "); scanf(" %79[^\n]", q);
    printf("\n  Results: %s\n", searchPatient(q));
}

/* ── Option 4: Visit history ── */
static void menuHistory(void)
{
    char pid[MAX_ID];
    dline();
    printf("  [ 4 ]  VIEW VISIT HISTORY\n");
    dline();
    printf("  Patient ID : "); scanf("%11s", pid);
    printf("\n  History: %s\n", getVisitHistory(pid));
}

/* ── Option 5: List all patients ── */
static void menuListAll(void)
{
    dline();
    printf("  [ 5 ]  ALL REGISTERED PATIENTS\n");
    dline();
    printf("%s\n", getAllPatients());
}

/* ── Option 6: Frequent visitors ── */
static void menuFrequent(void)
{
    dline();
    printf("  [ 6 ]  FREQUENT VISITORS  (>= %d visits)\n",
           FREQUENT_THRESHOLD);
    dline();
    printf("%s\n", getFrequentVisitors());
}

/* ── Option 7: Statistics ── */
static void menuStats(void)
{
    dline();
    printf("  [ 7 ]  SYSTEM STATISTICS\n");
    dline();
    printf("%s\n", getStats());
}

/* ── Option 8: Edit patient ── */
static void menuEdit(void)
{
    char id[MAX_ID], name[MAX_NAME], phone[MAX_PHONE];
    char gender[MAX_GENDER], blood[MAX_BLOOD], address[MAX_ADDR];
    char ageBuf[8];
    int  age, idx;

    dline();
    printf("  [ 8 ]  EDIT PATIENT RECORD\n");
    dline();
    printf("  Patient ID to edit : "); scanf("%11s", id);

    idx = findPatientIndex(id);
    if (idx < 0) {
        printf("  ERROR: Patient '%s' not found.\n", id);
        return;
    }

    printf("\n  Current: %s | Age: %d | Phone: %s | %s | %s\n",
           patients[idx].name, patients[idx].age,
           patients[idx].phone, patients[idx].gender, patients[idx].blood);
    printf("  (Press Enter alone to keep the current value)\n\n");

    /* Name */
    printf("  New Name   [%s] : ", patients[idx].name);
    scanf(" %79[^\n]", name);
    if (strlen(name) == 0) strncpy(name, patients[idx].name, MAX_NAME-1);

    /* Age */
    printf("  New Age    [%d] : ", patients[idx].age);
    scanf("%7s", ageBuf);
    age = (strlen(ageBuf) > 0) ? atoi(ageBuf) : patients[idx].age;

    /* Phone */
    printf("  New Phone  [%s] : ", patients[idx].phone);
    scanf("%11s", phone);
    if (strlen(phone) == 0) strncpy(phone, patients[idx].phone, MAX_PHONE-1);

    /* Gender */
    printf("  New Gender [%s] : ", patients[idx].gender);
    scanf("%15s", gender);
    if (strlen(gender) == 0) strncpy(gender, patients[idx].gender, MAX_GENDER-1);

    /* Blood */
    printf("  New Blood  [%s] : ", patients[idx].blood);
    scanf("%5s", blood);
    if (strlen(blood) == 0) strncpy(blood, patients[idx].blood, MAX_BLOOD-1);

    /* Address */
    printf("  New Address [%s] : ", patients[idx].address);
    scanf(" %119[^\n]", address);
    if (strlen(address) == 0) strncpy(address, patients[idx].address, MAX_ADDR-1);

    printf("\n  Result: %s\n",
           editPatient(id, name, age, phone, gender, blood, address));
}

/* ── Option 9: Delete patient ── */
static void menuDelete(void)
{
    char id[MAX_ID], confirm[8];
    int  idx;

    dline();
    printf("  [ 9 ]  DELETE PATIENT\n");
    dline();
    printf("  Patient ID to delete : "); scanf("%11s", id);

    idx = findPatientIndex(id);
    if (idx < 0) {
        printf("  ERROR: Patient '%s' not found.\n", id);
        return;
    }

    printf("\n  Patient  : %s (%s)\n",   patients[idx].name, id);
    printf("  Visits   : %d record(s) will also be deleted.\n",
           countVisits(id));
    printf("  WARNING  : This action cannot be undone!\n\n");
    printf("  Type  YES  to confirm  /  NO  to cancel : ");
    scanf("%7s", confirm);

    if (strcmp(confirm, "YES") == 0)
        printf("\n  Result: %s\n", deletePatient(id));
    else
        printf("  Deletion cancelled.\n");
}

/* ── main() – program entry point ── */
int main(void)
{
    int choice;

    loadFromFiles();

    printf("\n");
    dline();
    printf("  ╔══════════════════════════════════════════════╗\n");
    printf("  ║  MedTrack – Hospital Patient Mgmt System    ║\n");
    printf("  ║  DA1 · Question 8 · C Programming + WASM   ║\n");
    printf("  ╚══════════════════════════════════════════════╝\n");
    dline();

    do {
        printf("\n");
        printf("  ┌────────────────────────────────────────────┐\n");
        printf("  │                MAIN MENU                   │\n");
        printf("  ├────────────────────────────────────────────┤\n");
        printf("  │  1.  Register New Patient                  │\n");
        printf("  │  2.  Add Visit Record                      │\n");
        printf("  │  3.  Search Patient                        │\n");
        printf("  │  4.  View Visit History                    │\n");
        printf("  │  5.  List All Patients                     │\n");
        printf("  │  6.  Frequent Visitors  (>= 3 visits)     │\n");
        printf("  │  7.  System Statistics                     │\n");
        printf("  │  8.  Edit Patient Record                   │\n");
        printf("  │  9.  Delete Patient                        │\n");
        printf("  │  0.  Exit                                  │\n");
        printf("  └────────────────────────────────────────────┘\n");
        printf("  Choice : ");
        scanf("%d", &choice);

        printf("\n");

        switch (choice) {
            case 1:  menuRegisterPatient(); break;
            case 2:  menuAddVisit();        break;
            case 3:  menuSearch();          break;
            case 4:  menuHistory();         break;
            case 5:  menuListAll();         break;
            case 6:  menuFrequent();        break;
            case 7:  menuStats();           break;
            case 8:  menuEdit();            break;
            case 9:  menuDelete();          break;
            case 0:  printf("  Goodbye!\n\n"); break;
            default: printf("  Invalid choice. Enter 0–9.\n");
        }

    } while (choice != 0);

    return 0;
}

#else

/*
 * Emscripten main – runs once when the WASM module initialises.
 * Only loads saved data; the browser UI drives everything else
 * through the EXPORT functions above via Module.ccall().
 */
int main(void)
{
    loadFromFiles();
    return 0;
}

#endif /* __EMSCRIPTEN__ */

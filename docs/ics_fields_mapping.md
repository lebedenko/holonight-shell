# iCalendar (ICS) Fields Mapping Analysis

This report lists the fields defined in the iCalendar specification ([RFC 5545](https://datatracker.ietf.org/doc/html/rfc5545)) for a `VEVENT` component and compares them against the current implementation in the `holonight-shell` project.

All standard VEVENT fields are now parsed, mapped, cached, and exposed.

## 1. Mapped Fields

These properties are parsed from the ICS data in [ICalParser.cpp](file:///home/andrii/Projects/pet/holonight/holonight-shell/src/services/calendar/ICalParser.cpp#L268).

| Property (RFC 5545) | Parsed in Parser? | Mapped to `CalendarEvent`? | Cached in Database? | Exposed to QML? | Details / Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`UID`** | Yes (as `uid`) | Yes ([CalendarTypes.h](file:///home/andrii/Projects/pet/holonight/holonight-shell/src/services/calendar/CalendarTypes.h#L14)) | Yes (`uid` column) | Yes (`eventId` role) | Unique identifier. |
| **`SUMMARY`** | Yes (as `summary`) | Yes (mapped to `title`) | Yes (`title` column) | Yes (`title` role) | Short summary/subject of event. |
| **`DTSTART`** | Yes (as `dtstart_value`) | Yes (mapped to `start_time` & `is_all_day`) | Yes (`dtstart` and `is_all_day` columns) | Yes (`startTime` and `isAllDay` roles) | Start time. Sets `is_all_day` if type parameter is `VALUE=DATE`. |
| **`DTEND`** | Yes (as `dtend_value`) | Yes (mapped to `end_time`) | Yes (`dtend` column) | Yes (`endTime` role) | End time. |
| **`DESCRIPTION`** | Yes (as `description`) | Yes (mapped to `description`) | Yes (`description` column) | Yes (`description` role) | Description (unescaped during parsing). Truncated to 100 characters in QML model. |
| **`LOCATION`** | Yes (as `location`) | Yes (mapped to `location`) | Yes (`location` column) | Yes (`location` role) | Geographic location string. |
| **`DTSTAMP`** | Yes (as `dtstamp_value`) | Yes (mapped to `dtstamp`) | Yes (`dtstamp` column) | No | Date-time when the calendar object was created. Used in [IcsProvider.cpp](file:///home/andrii/Projects/pet/holonight/holonight-shell/src/services/calendar/IcsProvider.cpp#L66) for override-resolution. |
| **`RRULE`** | Yes (as `rrule`) | Yes (mapped to `rrule`) | Yes (`rrule` column) | Yes (`rrule` role) | Recurrence rule string. |
| **`DURATION`** | Yes (as `duration`) | Yes (mapped to `duration`) | Yes (`duration` column) | Yes (`duration` role) | Event duration. If `DTEND` is missing, `DURATION` calculates the event's end time. |
| **`CLASS`** | Yes (as `access_class`) | Yes (mapped to `access_class`) | Yes (`access_class` column) | Yes (`accessClass` role) | Access classification (PUBLIC, PRIVATE, CONFIDENTIAL). |
| **`CREATED`** | Yes (as `created_value`) | Yes (mapped to `created`) | Yes (`created` column) | Yes (`created` role) | Creation date-time. |
| **`LAST-MODIFIED`** | Yes (as `last_modified_value`) | Yes (mapped to `last_modified`) | Yes (`last_modified` column) | Yes (`lastModified` role) | Last revision date-time. |
| **`ORGANIZER`** | Yes (as `organizer`) | Yes (mapped to `organizer`) | Yes (`organizer` column) | Yes (`organizer` role) | Organizer CN and email details. |
| **`ATTENDEE`** | Yes (as `attendee` list) | Yes (mapped to `attendees`) | Yes (`attendees` column as JSON array) | Yes (`attendees` role) | List of attendees with parameters (RSVP, etc.). |
| **`CATEGORIES`** | Yes (split by commas) | Yes (mapped to `categories`) | Yes (`categories` column as JSON array) | Yes (`categories` role) | List of categories/tags. |
| **`STATUS`** | Yes (as `status`) | Yes (mapped to `status`) | Yes (`status` column) | Yes (`status` role) | Event status (TENTATIVE, CONFIRMED, CANCELLED). |
| **`TRANSP`** | Yes (as `transparency`) | Yes (mapped to `transparency`) | Yes (`transparency` column) | Yes (`transparency` role) | Time transparency (OPAQUE / TRANSPARENT). |
| **`URL`** | Yes (as `url`) | Yes (mapped to `url`) | Yes (`url` column) | Yes (`url` role) | URL pointing to external meeting/page info. |
| **`GEO`** | Yes (as `geo`) | Yes (mapped to `geo`) | Yes (`geo` column) | Yes (`geo` role) | Latitude and longitude string. |
| **`SEQUENCE`** | Yes (as `sequence`) | Yes (mapped to `sequence`) | Yes (`sequence` column) | Yes (`sequence` role) | Revision sequence number. |
| **`RECURRENCE-ID`** | Yes (as `recurrence_id`) | Yes (mapped to `recurrence_id`) | Yes (`recurrence_id` column) | Yes (`recurrenceId` role) | Instance ID for exceptions to recurrence patterns. |
| **`EXDATE`** | Yes (split by commas) | Yes (mapped to `exdates`) | Yes (`exdates` column as JSON array) | Yes (`exdates` role) | Recurrence exception dates. |
| **`RDATE`** | Yes (split by commas) | Yes (mapped to `rdates`) | Yes (`rdates` column as JSON array) | Yes (`rdates` role) | Extra recurrence dates. |
| **`ATTACH`** | Yes (as `attachments`) | Yes (mapped to `attachments`) | Yes (`attachments` column as JSON array) | Yes (`attachments` role) | List of document/media attachments. |
| **`COMMENT`** | Yes (as `comments`) | Yes (mapped to `comments`) | Yes (`comments` column as JSON array) | Yes (`comments` role) | Comments list. |
| **`CONTACT`** | Yes (as `contacts`) | Yes (mapped to `contacts`) | Yes (`contacts` column as JSON array) | Yes (`contacts` role) | Contact information list. |
| **`RELATED-TO`** | Yes (as `related_to`) | Yes (mapped to `related_to`) | Yes (`related_to` column as JSON array) | Yes (`relatedTo` role) | Reference links to other calendar components. |
| **`RESOURCES`** | Yes (split by commas) | Yes (mapped to `resources`) | Yes (`resources` column as JSON array) | Yes (`resources` role) | List of requested rooms or equipment. |
| **`VALARM`** | Yes (as raw alarm content lines) | Yes (mapped to `alarms`) | Yes (`alarms` column as JSON array) | Yes (`alarms` role) | List of parsed and serialized alarm sub-components. |

---

## 2. Unmapped / Ignored Fields

There are currently **no unmapped VEVENT fields** from the standard RFC 5545 iCalendar specification.

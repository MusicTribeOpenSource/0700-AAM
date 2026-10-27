/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

namespace BPM
{
    static constexpr float MIN{ 20 };
    static constexpr float MAX{ 240 };
    static constexpr float STEP{ 1 };
    static constexpr float DEFAULT{ 120 };
    static constexpr float TOSEC{ 60 };

}

namespace BAR
{
    static constexpr int MIN{ 1 };
    static constexpr int MAX{ 100 };
    static constexpr int LOOP_RECORDER_MAX{ 16 };
    static constexpr int STEP{ 1 };
    static constexpr int DEFAULT{ 4 };
}

namespace RATCHET
{
    static constexpr int MIN{ 0 };
    static constexpr int MAX{ 8 };
    static constexpr int STEP{ 1 };
    static constexpr int DEFAULT{ 0 };
}

namespace PROBABILITY
{
    static constexpr int DEFAULT{ 100 };
    static constexpr int MIN{ 0 };
    static constexpr int MAX{ 100 };
}

namespace TRANSPOSE
{
    static constexpr int MIN{ -36 };
    static constexpr int MAX{ 36 };
    static constexpr int STEP{ 1 };
    static constexpr int DEFAULT{ 0 };
}

namespace SEMI
{
    static constexpr int MIN{ -12 };
    static constexpr int MAX{ 12 };
    static constexpr int STEP{ 1 };
    static constexpr int DEFAULT{ 0 };
}

namespace FINE
{
    static constexpr int MIN{ -99 };
    static constexpr int MAX{ 99 };
    static constexpr int STEP{ 1 };
    static constexpr int DEFAULT{ 0 };
}

namespace MUTEGROUP
{
    static constexpr int MIN{ -1 };
    static constexpr int MAX{ 32 };
    static constexpr int STEP{ 1 };
    static constexpr int DEFAULT{ -1 };
}

namespace INSERTS
{
    static constexpr float MAX{ 4 };
}

namespace QUANTIZE
{
    static constexpr float MIN{ 0.0416666667f }; // 1/32T Note.
    static constexpr float MAX{ 4.f }; // 1 Bar
    static constexpr float DEFAULT{ 0.25f }; // 1/4 Note.
}

namespace EMPTY
{
    inline const juce::String String{};
    inline const juce::AudioBuffer<float> SampleBuffer{};
    static const juce::ValueTree VTState{};
}

namespace CONSTANTS
{
    const static std::vector<juce::String> timeZoneList = {
        "Africa/Abidjan", "Africa/Accra", "Africa/Algiers", "Africa/Bissau", "Africa/Cairo",
        "Africa/Casablanca", "Africa/Ceuta", "Africa/El_Aaiun", "Africa/Johannesburg", "Africa/Juba",
        "Africa/Khartoum", "Africa/Lagos", "Africa/Maputo", "Africa/Monrovia", "Africa/Nairobi",
        "Africa/Ndjamena", "Africa/Sao_Tome", "Africa/Tripoli", "Africa/Tunis", "Africa/Windhoek",
        "America/Adak", "America/Anchorage", "America/Araguaina", "America/Argentina/Buenos_Aires",
        "America/Argentina/Catamarca", "America/Argentina/Cordoba", "America/Argentina/Jujuy",
        "America/Argentina/La_Rioja", "America/Argentina/Mendoza", "America/Argentina/Rio_Gallegos",
        "America/Argentina/Salta", "America/Argentina/San_Juan", "America/Argentina/San_Luis",
        "America/Argentina/Tucuman", "America/Argentina/Ushuaia", "America/Asuncion", "America/Atikokan",
        "America/Bahia", "America/Bahia_Banderas", "America/Barbados", "America/Belem", "America/Belize",
        "America/Blanc-Sablon", "America/Boa_Vista", "America/Bogota", "America/Boise", "America/Cambridge_Bay",
        "America/Campo_Grande", "America/Cancun", "America/Caracas", "America/Cayenne", "America/Chicago",
        "America/Chihuahua", "America/Costa_Rica", "America/Creston", "America/Cuiaba", "America/Curacao",
        "America/Danmarkshavn", "America/Dawson", "America/Dawson_Creek", "America/Denver", "America/Detroit",
        "America/Edmonton", "America/Eirunepe", "America/El_Salvador", "America/Fort_Nelson", "America/Fortaleza",
        "America/Glace_Bay", "America/Goose_Bay", "America/Grand_Turk", "America/Guatemala", "America/Guayaquil",
        "America/Guyana", "America/Halifax", "America/Havana", "America/Hermosillo", "America/Indiana/Indianapolis",
        "America/Indiana/Knox", "America/Indiana/Marengo", "America/Indiana/Petersburg", "America/Indiana/Tell_City",
        "America/Indiana/Vevay", "America/Indiana/Vincennes", "America/Indiana/Winamac", "America/Inuvik",
        "America/Iqaluit", "America/Jamaica", "America/Juneau", "America/Kentucky/Louisville", "America/Kentucky/Monticello",
        "America/La_Paz", "America/Lima", "America/Los_Angeles", "America/Maceio", "America/Managua",
        "America/Manaus", "America/Martinique", "America/Matamoros", "America/Mazatlan", "America/Menominee",
        "America/Merida", "America/Metlakatla", "America/Mexico_City", "America/Miquelon", "America/Moncton",
        "America/Monterrey", "America/Montevideo", "America/Nassau", "America/New_York", "America/Nipigon",
        "America/Nome", "America/Noronha", "America/North_Dakota/Beulah", "America/North_Dakota/Center",
        "America/North_Dakota/New_Salem", "America/Nuuk", "America/Ojinaga", "America/Panama", "America/Pangnirtung",
        "America/Paramaribo", "America/Phoenix", "America/Port-au-Prince", "America/Port_of_Spain", "America/Porto_Velho",
        "America/Puerto_Rico", "America/Punta_Arenas", "America/Rainy_River", "America/Rankin_Inlet", "America/Recife",
        "America/Regina", "America/Resolute", "America/Rio_Branco", "America/Santarem", "America/Santiago",
        "America/Santo_Domingo", "America/Sao_Paulo", "America/Scoresbysund", "America/Sitka", "America/St_Johns",
        "America/Swift_Current", "America/Tegucigalpa", "America/Thule", "America/Thunder_Bay", "America/Tijuana",
        "America/Toronto", "America/Vancouver", "America/Whitehorse", "America/Winnipeg", "America/Yakutat",
        "America/Yellowknife",
        "Antarctica/Casey", "Antarctica/Davis", "Antarctica/DumontDUrville", "Antarctica/Macquarie", "Antarctica/Mawson",
        "Antarctica/Palmer", "Antarctica/Rothera", "Antarctica/Syowa", "Antarctica/Troll", "Antarctica/Vostok",
        "Asia/Almaty", "Asia/Amman", "Asia/Anadyr", "Asia/Aqtau", "Asia/Aqtobe",
        "Asia/Ashgabat", "Asia/Atyrau", "Asia/Baghdad", "Asia/Baku", "Asia/Bangkok",
        "Asia/Barnaul", "Asia/Beirut", "Asia/Bishkek", "Asia/Brunei", "Asia/Chita",
        "Asia/Choibalsan", "Asia/Colombo", "Asia/Damascus", "Asia/Dhaka", "Asia/Dili",
        "Asia/Dubai", "Asia/Dushanbe", "Asia/Famagusta", "Asia/Gaza", "Asia/Hebron",
        "Asia/Ho_Chi_Minh", "Asia/Hong_Kong", "Asia/Hovd", "Asia/Irkutsk", "Asia/Jakarta",
        "Asia/Jayapura", "Asia/Jerusalem", "Asia/Kabul", "Asia/Kamchatka", "Asia/Karachi",
        "Asia/Kathmandu", "Asia/Khandyga", "Asia/Kolkata", "Asia/Krasnoyarsk", "Asia/Kuala_Lumpur",
        "Asia/Kuching", "Asia/Macau", "Asia/Magadan", "Asia/Makassar", "Asia/Manila",
        "Asia/Nicosia", "Asia/Novokuznetsk", "Asia/Novosibirsk", "Asia/Omsk", "Asia/Oral",
        "Asia/Pontianak", "Asia/Pyongyang", "Asia/Qatar", "Asia/Qyzylorda", "Asia/Riyadh",
        "Asia/Sakhalin", "Asia/Samarkand", "Asia/Seoul", "Asia/Shanghai", "Asia/Singapore",
        "Asia/Srednekolymsk", "Asia/Taipei", "Asia/Tashkent", "Asia/Tbilisi", "Asia/Tehran",
        "Asia/Thimphu", "Asia/Tokyo", "Asia/Tomsk", "Asia/Ulaanbaatar", "Asia/Urumqi",
        "Asia/Ust-Nera", "Asia/Vladivostok", "Asia/Yakutsk", "Asia/Yangon", "Asia/Yekaterinburg",
        "Asia/Yerevan", "Atlantic/Azores", "Atlantic/Bermuda", "Atlantic/Canary", "Atlantic/Cape_Verde",
        "Atlantic/Faroe", "Atlantic/Madeira", "Atlantic/Reykjavik", "Atlantic/South_Georgia", "Atlantic/Stanley",
        "Australia/Adelaide", "Australia/Brisbane", "Australia/Broken_Hill", "Australia/Darwin", "Australia/Eucla",
        "Australia/Hobart", "Australia/Lindeman", "Australia/Lord_Howe", "Australia/Melbourne", "Australia/Perth",
        "Australia/Sydney", "Europe/Amsterdam", "Europe/Andorra", "Europe/Astrakhan", "Europe/Athens",
        "Europe/Belgrade", "Europe/Berlin", "Europe/Brussels", "Europe/Bucharest", "Europe/Budapest",
        "Europe/Chisinau", "Europe/Copenhagen", "Europe/Dublin", "Europe/Gibraltar", "Europe/Helsinki",
        "Europe/Istanbul", "Europe/Kaliningrad", "Europe/Kiev", "Europe/Kirov", "Europe/Lisbon",
        "Europe/Ljubljana", "Europe/London", "Europe/Luxembourg", "Europe/Madrid", "Europe/Malta",
        "Europe/Minsk", "Europe/Monaco", "Europe/Moscow", "Europe/Oslo", "Europe/Paris",
        "Europe/Prague", "Europe/Riga", "Europe/Rome", "Europe/Samara", "Europe/Saratov",
        "Europe/Simferopol", "Europe/Sofia", "Europe/Stockholm", "Europe/Tallinn", "Europe/Tirane",
        "Europe/Ulyanovsk", "Europe/Uzhgorod", "Europe/Vienna", "Europe/Vilnius", "Europe/Volgograd",
        "Europe/Warsaw", "Europe/Zagreb", "Europe/Zaporozhye", "Europe/Zurich", "Indian/Chagos",
        "Indian/Christmas", "Indian/Cocos", "Indian/Kerguelen", "Indian/Mahe", "Indian/Maldives",
        "Indian/Mauritius", "Indian/Reunion", "Pacific/Apia", "Pacific/Auckland", "Pacific/Bougainville",
        "Pacific/Chatham", "Pacific/Chuuk", "Pacific/Easter", "Pacific/Efate", "Pacific/Enderbury",
        "Pacific/Fakaofo", "Pacific/Fiji", "Pacific/Funafuti", "Pacific/Galapagos", "Pacific/Gambier",
        "Pacific/Guadalcanal", "Pacific/Guam", "Pacific/Honolulu", "Pacific/Kiritimati", "Pacific/Kosrae",
        "Pacific/Kwajalein", "Pacific/Majuro", "Pacific/Marquesas", "Pacific/Nauru", "Pacific/Niue",
        "Pacific/Norfolk", "Pacific/Noumea", "Pacific/Pago_Pago", "Pacific/Palau", "Pacific/Pitcairn",
        "Pacific/Pohnpei", "Pacific/Port_Moresby", "Pacific/Rarotonga", "Pacific/Saipan", "Pacific/Tahiti",
        "Pacific/Tarawa", "Pacific/Tongatapu", "Pacific/Wake", "Pacific/Wallis", "UTC"
    };
}

//==============================================================================

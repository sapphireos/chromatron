/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2022  Jeremy Billheimer
// 
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// </license>
 */


#include "system.h"
#include "datetime.h"

#include <stdlib.h>

char* itoa(int value, char* result, int base);

static const uint8_t PROGMEM days_per_month_table[MONTHS_PER_YEAR] = {
	31, // january
	28, // february - non-leap years
	31, // march
	30, // april
	31, // may
	30, // june
	31, // july
	31, // august
	30, // september
	31, // october
	30, // november
	31, // december
};


// initialize the timekeeping module
void datetime_v_init( void ){

}


// API functions for datetime data structures

static uint8_t ascii_to_digit( char c ){

	return c - 48;
}

// "2010-05-16T13:28:17-05:00"

void datetime_v_parse_iso8601( char *iso8601, uint8_t len, datetime_t *datetime ){

	datetime_v_get_epoch( datetime );

	if( len >= ISO8601_STRING_MIN_LEN ){

		datetime->year = ascii_to_digit( iso8601[0] ) * 1000;
		datetime->year += ascii_to_digit( iso8601[1] ) * 100;

		datetime->year += ascii_to_digit( iso8601[2] ) * 10;
		datetime->year += ascii_to_digit( iso8601[3] );

		datetime->month = ascii_to_digit( iso8601[5] ) * 10;
		datetime->month += ascii_to_digit( iso8601[6] );

		datetime->day = ascii_to_digit( iso8601[8] ) * 10;
		datetime->day += ascii_to_digit( iso8601[9] );

		datetime->hours = ascii_to_digit( iso8601[11] ) * 10;
		datetime->hours += ascii_to_digit( iso8601[12] );

		datetime->minutes = ascii_to_digit( iso8601[14] ) * 10;
		datetime->minutes += ascii_to_digit( iso8601[15] );

		datetime->seconds = ascii_to_digit( iso8601[17] ) * 10;
		datetime->seconds += ascii_to_digit( iso8601[18] );
	}
}

// "2010-05-16T13:28:17"
// timezone is skipped
void datetime_v_to_iso8601( char *iso8601, uint8_t len, datetime_t *datetime ){

	// bounds check
	ASSERT( len >= ISO8601_STRING_MIN_LEN );

	// NOTE: this will only work properly for years greater than 1000.
	// Otherwise, the string will not be long enough and the year
	// string will be incorrect
	itoa( datetime->year, iso8601, 10 );

	iso8601[4] = '-';

	itoa( datetime->month, &iso8601[5], 10 );

	// adjust for less than 10
	if( datetime->month < 10 ){

		iso8601[6] = iso8601[5];
		iso8601[5] = '0';
	}

	iso8601[7] = '-';

	itoa( datetime->day, &iso8601[8], 10 );

	// adjust for less than 10
	if( datetime->day < 10 ){

		iso8601[9] = iso8601[8];
		iso8601[8] = '0';
	}

	iso8601[10] = 'T';

	itoa( datetime->hours, &iso8601[11], 10 );

	// adjust for less than 10
	if( datetime->hours < 10 ){

		iso8601[12] = iso8601[11];
		iso8601[11] = '0';
	}

	iso8601[13] = ':';

	itoa( datetime->minutes, &iso8601[14], 10 );

	// adjust for less than 10
	if( datetime->minutes < 10 ){

		iso8601[15] = iso8601[14];
		iso8601[14] = '0';
	}

	iso8601[16] = ':';

	itoa( datetime->seconds, &iso8601[17], 10 );

	// adjust for less than 10
	if( datetime->seconds < 10 ){

		iso8601[18] = iso8601[17];
		iso8601[17] = '0';
	}

	iso8601[19] = 0;
}

// set the given datetime to the lowest valid date and time in the epoch
void datetime_v_get_epoch( datetime_t *datetime ){

	datetime->seconds 		= 0;
	datetime->minutes 		= 0;
	datetime->hours			= 0;
	datetime->day			= 1;
	datetime->weekday		= MONDAY;
	datetime->month			= 1;
	datetime->year			= 1900;
}

// calculates datetime from seconds starting at Midnight January 1, 1900 (NTP epoch)
void datetime_v_seconds_to_datetime( uint32_t seconds, datetime_t *datetime ){

    // get number of days
    uint16_t days = seconds / SECONDS_PER_DAY;

 	// January 1, 1900 is a Monday.
    // Monday is a 1, but we are offsetting by -1 to start at 0 so we can use modulo
    // arithmetic to get the current day of week.
    // then we'll add 1 back.
    datetime->weekday = ( days % 7 ) + 1;

    // get year
    datetime->year = 1900 - 1;
    uint16_t temp_days = 0;

    while( temp_days <= days ){

        datetime->year++;
        temp_days += DAYS_PER_YEAR;

        // check for leap year
        if( datetime_b_is_leap_year( datetime ) ){

            temp_days++;
        }
    }

    temp_days -= DAYS_PER_YEAR;

    // check leap year again
    if( datetime_b_is_leap_year( datetime ) ){

        temp_days--;
    }

    uint16_t day_of_year = days - temp_days;
    seconds -= ( (uint32_t)temp_days * (uint32_t)SECONDS_PER_DAY );

    // calculate month
    for( uint8_t month = JANUARY; month <= MONTHS_PER_YEAR; month++ ){

        uint8_t days_per_month = pgm_read_byte( &days_per_month_table[month - 1] );

        if( ( month == FEBRUARY ) && datetime_b_is_leap_year( datetime ) ){

            days_per_month = FEBRUARY_LEAP_YEAR_DAYS;
        }

        if( day_of_year < days_per_month ){

            datetime->month = month;
            datetime->day = day_of_year + 1;

            seconds -= ( (uint32_t)day_of_year * SECONDS_PER_DAY );

            break;
        }

        day_of_year -= days_per_month;
        seconds -= ( (uint32_t)days_per_month * SECONDS_PER_DAY );
    }

    // hours
    datetime->hours = ( seconds / ( (uint32_t)SECONDS_PER_MINUTE * (uint32_t)MINUTES_PER_HOUR ) );
    seconds -= ( datetime->hours * ( (uint32_t)SECONDS_PER_MINUTE * (uint32_t)MINUTES_PER_HOUR ) );

    // minutes
    datetime->minutes = seconds / (uint32_t)SECONDS_PER_MINUTE;
    seconds -= ( datetime->minutes * (uint32_t)SECONDS_PER_MINUTE );

    // seconds
    datetime->seconds = seconds;
}

uint32_t datetime_u32_datetime_to_seconds( const datetime_t *datetime ){

	datetime_t datetime_copy = *datetime;
	
	uint16_t temp_days = 0;

	// compute total days from year
	while( datetime_copy.year > 1900 ){

		temp_days += DAYS_PER_YEAR;

		// check for leap year
        if( datetime_b_is_leap_year( &datetime_copy ) ){

            temp_days++;
        }

        datetime_copy.year--;
	}

    // check for leap year
    if( datetime_b_is_leap_year( datetime ) ){

        temp_days--;
    }

	// calculate days from month
    for( uint8_t month = JANUARY; month < datetime->month; month++ ){

        uint8_t days_per_month = pgm_read_byte( &days_per_month_table[month - 1] );

        if( ( month == FEBRUARY ) && datetime_b_is_leap_year( datetime ) ){

            days_per_month = FEBRUARY_LEAP_YEAR_DAYS;
        }

        temp_days += days_per_month;
    }	

    temp_days += ( datetime->day - 1 );

    uint32_t seconds = temp_days * SECONDS_PER_DAY;

    seconds += ( (uint32_t)datetime->hours * MINUTES_PER_HOUR * SECONDS_PER_MINUTE );
    seconds += ( (uint32_t)datetime->minutes * SECONDS_PER_MINUTE );
    seconds += datetime->seconds;

    return seconds;
}

void datetime_v_increment_seconds( datetime_t *datetime ){

	datetime->seconds++;

	if( datetime->seconds >= SECONDS_PER_MINUTE ){

		datetime->seconds = 0;

		datetime->minutes++;

		if( datetime->minutes >= MINUTES_PER_HOUR ){

			datetime->minutes = 0;

			datetime->hours++;

			if( datetime->hours >= HOURS_PER_DAY ){

				datetime->hours = 0;

				datetime->day++;
				datetime->weekday++;

				if( datetime->weekday > SUNDAY ){

					datetime->weekday = MONDAY;
				}

				uint8_t days_per_month;

				// check if month is february and it
				// is a leap year (inverted logic for efficiency)
				if( !( ( datetime->month == FEBRUARY ) &&
					 ( datetime_b_is_leap_year( datetime ) == TRUE ) ) ){

					days_per_month = pgm_read_byte( &days_per_month_table[ datetime->month - 1 ] );
				}
				else{

					days_per_month = FEBRUARY_LEAP_YEAR_DAYS;
				}

				if( datetime->day > days_per_month ){

					datetime->day = 1;

					datetime->month++;

					if( datetime->month > MONTHS_PER_YEAR ){

						datetime->year++;

						datetime->month = 1;
					}
				}
			}
		}
	}
}

bool datetime_b_is_leap_year( const datetime_t *datetime ){

	if( ( datetime->year % 4 ) == 0 ){

		if( ( datetime->year % 100 ) == 0 ){

			if( ( datetime->year % 400 ) == 0 ){

				return TRUE;
			}
			else{

				return FALSE;
			}
		}
		else{

			return TRUE;
		}
	}

	return FALSE;
}

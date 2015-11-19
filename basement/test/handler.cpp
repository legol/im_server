#include "handler.h"
#include <pthread.h>
#include <math.h>

#include "main.h"

CHandler::CHandler()
{
}

CHandler::~CHandler()
{
}

int32_t	CHandler::Init()
{
	return 0;
}

int32_t	CHandler::Process( tagHttpEvent item )
{
	switch( item.eEventType )
	{
		case EVENT_TYPE_PACKET_RECEIVED:
			{
			}
			break;
		case EVENT_TYPE_PACKET_SENT:
			{
			}
			break;
		case EVENT_TYPE_PACKET_TIMEOUT:
			{
			}
			break;
		case EVENT_TYPE_CONNECTED:
			{
			}
			break;
		case EVENT_TYPE_CLOSE:
			{
			}
			break;
		default:
			{
				// 不会进入这里
			}
			break;
	}

	return 0;
}


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

 // Copyright (c) 2024 MUSIC Tribe. SPDX-License-Identifier: AGPL-3.0-or-later

#include "juce_core/juce_core.h"
#include "mt_ipc_json.h"
#include "mt_ipc_exception.h"
#include "mt_ipc_error_codes.h"
#include <assert.h>

namespace ipc
{
	class MTIPCJsonImpl
	{
	public:
		MTIPCJsonImpl()
			: _var_(new juce::DynamicObject()) {}
		MTIPCJsonImpl(const juce::String& jsonString)
		{
			if (jsonString.isNotEmpty())
			{
				parse_string(jsonString);
			}
		}
		MTIPCJsonImpl(const MTIPCJsonImpl& rhs)
			: _var_(rhs._var_) {}

		MTIPCJsonImpl& operator=(const MTIPCJsonImpl& rhs)
		{
			_var_ = rhs._var_;
			return *this;
		}

	public:

		void parse_file(const juce::String& absFilePath)
		{
			if (absFilePath.isEmpty())
			{
				throw ipc::MTIPCException(ipc::EMPTY_FILE_NAME_ERROR_CODE, ipc::EMPTY_FILE_NAME_ERROR_MESSAGE);
			}

			try
			{
				//const juce::ScopedWriteLock locker( JSON_METHOD_LOCK );
				_var_ = juce::JSON::parse(juce::File(absFilePath));
			}
			catch (const std::exception& ex)
			{
				throw ipc::MTIPCException(ipc::EMPTY_FILE_NAME_ERROR_CODE,
					ipc::EMPTY_FILE_NAME_ERROR_MESSAGE + std::string(ex.what()));
			}
			catch (...)
			{
				throw ipc::MTIPCException(ipc::EMPTY_FILE_NAME_ERROR_CODE, ipc::EMPTY_FILE_NAME_ERROR_MESSAGE);
			}
		}

		void parse_string(const juce::String& jsonStr)
		{
			if (jsonStr.isEmpty())
			{
				throw ipc::MTIPCException(ipc::EMPTY_MESSAGE_ERROR_CODE, ipc::EMPTY_MESSAGE_ERROR_MESSAGE);
			}

			try
			{
				// const juce::ScopedWriteLock locker( JSON_METHOD_LOCK );
				_var_ = juce::JSON::parse(jsonStr);
			}
			catch (const std::exception& ex)
			{
				throw ipc::MTIPCException(ipc::EMPTY_MESSAGE_ERROR_CODE,
					ipc::EMPTY_MESSAGE_ERROR_MESSAGE + std::string(ex.what()));
			}
			catch (...)
			{
				throw ipc::MTIPCException(ipc::EMPTY_MESSAGE_ERROR_CODE, ipc::EMPTY_MESSAGE_ERROR_MESSAGE);
			}
		}

		void setProperty(const juce::String& dotSeparatedKey, const juce::var& value)
		{
			try
			{
				juce::String key;
				int          start = 0;
				int          index = 0;
				{
					// const juce::ScopedWriteLock locker( JSON_METHOD_LOCK );
					juce::DynamicObject* obj = _var_.getDynamicObject();
					while ((index = dotSeparatedKey.indexOfChar(start, '.')) != -1)
					{
						key = dotSeparatedKey.substring(start, index);
						if (obj == nullptr)
						{
							// the root, or an intermediate key, holds a value that is not an object
							throw ipc::MTIPCException(ipc::JSON_SET_VALUE_ERROR_CODE, ipc::JSON_SET_VALUE_ERROR_MESSAGE);
						}
						if (!obj->hasProperty(key))
						{
							obj->setProperty(key, new juce::DynamicObject());
						}
						obj = obj->getProperty(key).getDynamicObject();

						start = index + 1;
					}

					if (obj == nullptr)
					{
						throw ipc::MTIPCException(ipc::JSON_SET_VALUE_ERROR_CODE, ipc::JSON_SET_VALUE_ERROR_MESSAGE);
					}
					key = dotSeparatedKey.substring(start, dotSeparatedKey.length());
					obj->setProperty(key, value);
				}

			}
			catch (std::exception ex)
			{
				throw ipc::MTIPCException(ipc::JSON_SET_VALUE_ERROR_CODE,
					ipc::JSON_SET_VALUE_ERROR_MESSAGE + std::string(ex.what()));
			}
			catch (...)
			{
				throw ipc::MTIPCException(ipc::JSON_SET_VALUE_ERROR_CODE, ipc::JSON_SET_VALUE_ERROR_MESSAGE);
			}
		}

		const juce::var& getProperty(const juce::String& dotSeparatedKey, const juce::var& defaultValue)
		{
			try
			{
				juce::String key;
				int          start = 0;
				int          index = 0;
				{
					// const juce::ScopedReadLock locker( JSON_METHOD_LOCK );
					if (dotSeparatedKey.isEmpty())
					{
						return _var_;
					}

					juce::DynamicObject* obj = _var_.getDynamicObject();
					while ((index = dotSeparatedKey.indexOfChar(start, '.')) != -1)
					{
						key = dotSeparatedKey.substring(start, index);
						// A missing key, or an intermediate value that is not an object, ends the walk:
						// hand back the default (or raise when the caller gave none).
						if (obj == nullptr || !obj->hasProperty(key))
						{
							return _returnDefaultValueOrRaiseException_(defaultValue);
						}
						obj = obj->getProperty(key).getDynamicObject();

						start = index + 1;
					}

					key = dotSeparatedKey.substring(start, dotSeparatedKey.length());
					return _getValue_(obj, key, defaultValue);
				}

			}
			catch (const ipc::MTIPCException&)
			{
				throw;
			}
			catch (std::exception ex)
			{
				throw ipc::MTIPCException(ipc::JSON_GET_VALUE_ERROR_CODE,
					ipc::JSON_GET_VALUE_ERROR_MESSAGE + std::string(ex.what()));
			}
			catch (...)
			{
				throw ipc::MTIPCException(ipc::JSON_GET_VALUE_ERROR_CODE, ipc::JSON_GET_VALUE_ERROR_MESSAGE);
			}
		}

		void copy(const ipc::MTIPCJsonImpl& rhs)
		{
			_var_ = rhs._var_.clone();
		}

		std::unique_ptr<const juce::String> toString()
		{
			return std::make_unique<const juce::String>(juce::JSON::toString(_var_));
		}

		const juce::var& getRoot()
		{
			return _var_;
		}

	private:
		const juce::var& _getValue_(
			juce::DynamicObject* obj,
			const juce::String& key,
			const juce::var& defaultValue)
		{
			std::unique_ptr<juce::var> ret;
			if (obj && obj->hasProperty(key))
			{
				return obj->getProperty(key);
			}

			return _returnDefaultValueOrRaiseException_(defaultValue);
		}

		const juce::var& _returnDefaultValueOrRaiseException_(const juce::var& defaultValue)
		{
			if (defaultValue.isUndefined()) // if default value is undefined then raise exception.
			{
				throw ipc::MTIPCException(ipc::JSON_GET_VALUE_ERROR_CODE,
					ipc::JSON_GET_VALUE_ERROR_MESSAGE);
			}
			else
			{
				return defaultValue;
			}
		}

	private:
		juce::var _var_;
		//static juce::ReadWriteLock JSON_METHOD_LOCK; // enable locks if necessary, now json message is accessed sequentially across threads.
	};
}

// only one lock is used instead of locks for each method,
// as it is necessary for a threads to wait till the data is updated.
//juce::ReadWriteLock ipc::MTIPCJsonImpl::JSON_METHOD_LOCK = juce::ReadWriteLock();

namespace ipc
{
	MTIPCJson::MTIPCJson()
		: _impl_(new ipc::MTIPCJsonImpl()) {}
	MTIPCJson::MTIPCJson(const juce::String& jsonString)
		: _impl_(new ipc::MTIPCJsonImpl(jsonString)) {}

	// does shallow copy.call copy method to do a deepcopy
	MTIPCJson::MTIPCJson(const MTIPCJson& copy)
		: _impl_(new ipc::MTIPCJsonImpl(*copy._impl_)) {}

	// does shallow copy. call copy method to do a deepcopy
	MTIPCJson& MTIPCJson::operator=(const MTIPCJson& copy)
	{
		*_impl_ = *copy._impl_;
		return *this;
	}
	MTIPCJson::~MTIPCJson()
	{
		_impl_.reset();
	}

	void MTIPCJson::copy(const ipc::MTIPCJson& rhs)
	{
		_impl_->copy(*rhs._impl_);
	}

	void MTIPCJson::parseFile(const juce::String& absFilePath)
	{
		_impl_->parse_file(absFilePath);
	}

	void MTIPCJson::parseString(const juce::String& jsonStr)
	{
		_impl_->parse_string(jsonStr);
	}

	void MTIPCJson::setProperty(const juce::String& dotSeparatedKey, const juce::var& value)
	{
		_impl_->setProperty(dotSeparatedKey, value);
	}

	const juce::var& MTIPCJson::getProperty(const juce::String& dotSeparatedKey,
		const juce::var& defultValue)
	{
		return _impl_->getProperty(dotSeparatedKey, defultValue);
	}

	const juce::var& MTIPCJson::getProperty(const juce::String& dotSeparatedKey)
	{
		return _impl_->getProperty(dotSeparatedKey, juce::var::undefined());
	}

	const juce::var& MTIPCJson::getRoot()
	{
		return _impl_->getRoot();
	}
}

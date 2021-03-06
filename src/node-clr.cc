#include "node-clr.h"

using namespace v8;
using namespace System::IO;
using namespace System::Reflection;
using namespace System::Text::RegularExpressions;

/*
 * main module
 */
class CLR
{
	// clr.import(assemblyName | assemblyPath)
	//   load specified assembly into current process.
	//   - assemblyName: partial name of assembly, ie) "System.Data"
	//   - assemblyPath: .NET EXE/DLL file path relative from cwd
	static NAN_METHOD(Import)
	{
		NanScope();

		if (args.Length() != 1 || !args[0]->IsString())
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}

		auto name = ToCLRString(args[0]);
		Assembly^ assembly;
		try
		{
			if (File::Exists(name))
			{
				assembly = Assembly::LoadFrom(name);
			}
			else
			{
#pragma warning(push)
#pragma warning(disable:4947)
				assembly = Assembly::LoadWithPartialName(name);
#pragma warning(pop)
			}
		}
		catch (System::Exception^ ex)
		{
			NanThrowError(ToV8Error(ex));
			NanReturnUndefined();
		}

		if (assembly == nullptr)
		{
			NanThrowError("Assembly not found");
			NanReturnUndefined();
		}

		NanReturnUndefined();
	}
	

	// clr.getAssemblies() : assemblyNames
	//   lists all assembly names in current process
	//   - assemblyNames: array of assembly name string
	static NAN_METHOD(GetAssemblies)
	{
		NanScope();

		if (args.Length() != 0)
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}

		auto arr = NanNew<Array>();
		auto index = 0;
		for each (auto assembly in System::AppDomain::CurrentDomain->GetAssemblies())
		{
			if (assembly == Assembly::GetExecutingAssembly())
			{
				continue;
			}

			arr->Set(NanNew<Number>(index++), ToV8String(assembly->FullName));
		}

		NanReturnValue(arr);
	}
	

	// clr.getTypes() : typeNames
	//   lists all non-nested type name (Assembly-Qualified-Name) in current process
	//   - typeNames: array of type name string
	static NAN_METHOD(GetTypes)
	{
		NanScope();

		if (args.Length() != 0)
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}

		auto arr = NanNew<Array>();
		auto index = 0;
		for each (auto assembly in System::AppDomain::CurrentDomain->GetAssemblies())
		{
			// exclude current assembly ("clr.node")
			if (assembly == System::Reflection::Assembly::GetExecutingAssembly())
			{
				continue;
			}

			for each (auto type in assembly->GetTypes())
			{
				// exclude non-public types
				if (!type->IsPublic)
				{
					continue;
				}
				// exclude compiler generated types
				if (type->IsSpecialName)
				{
					continue;
				}

				arr->Set(NanNew<Number>(index++), ToV8String(type->AssemblyQualifiedName));
			}
		}

		NanReturnValue(arr);
	}
	

	// clr.createConstructor(typeName, initializer) : constructor
	//   create new constructor function from given typeName,
	//   - typeName: type name of constructor
	//   - initializer: an function which is invoked in constructor function
	//   - constructor: an constructor function to invoke CLR type constructor, returning CLR wrapped function
	static NAN_METHOD(CreateConstructor)
	{
		NanScope();

		if ((args.Length() != 1 && args.Length() != 2) ||
			!args[0]->IsString() ||
			!args[1]->IsFunction())
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}

		Handle<Value> result;
		try
		{
			result = CLRObject::CreateConstructor(
				Handle<String>::Cast(args[0]),
				Handle<Function>::Cast(args[1]));
		}
		catch (System::Exception^ ex)
		{
			NanThrowError(ToV8Error(ex));
			NanReturnUndefined();
		}

		NanReturnValue(result);
	}
	

	// clr.getMembers(typeName, CLRObject) : members
	//   list up type's static or instance members
	//   - typeName: type name string
	//   - CLRObject: CLR object instance or null
	//   - members: array of object that contains member information
	//   - members[i].memberType: 'event' | 'field' | 'method' | 'property' | 'nestedType'
	//   - members[i].name: member name
	//   - members[i].accessibility: array of string which denotes member's accessibility, 'get' | 'set'
	//   - members[i].fullName: CLR type's full name for nestedType
	static NAN_METHOD(GetMembers)
	{
		NanScope();

		if (args.Length() != 2 ||
			!args[0]->IsString())
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}

		auto type = System::Type::GetType(ToCLRString(args[0]), true);
		auto isStatic = !args[1]->BooleanValue();
		
		auto obj = NanNew<Object>();
		auto members = type->GetMembers(
			BindingFlags::Public |
			((isStatic) ? BindingFlags::Static : BindingFlags::Instance));
		for each (auto member in members)
		{
			auto ei = dynamic_cast<EventInfo^>(member);
			if (ei != nullptr &&
				!ei->IsSpecialName &&
				!obj->Has(ToV8Symbol(member->Name)))
			{
				// events
				auto desc = NanNew<Object>();
				desc->Set(NanNew<String>("name"), ToV8String(member->Name));
				desc->Set(NanNew<String>("type"), NanNew<String>("event"));
				obj->Set(ToV8Symbol(member->Name), desc);
			}

			auto fi = dynamic_cast<FieldInfo^>(member);
			if (fi != nullptr &&
				!fi->IsSpecialName &&
				!obj->Has(ToV8Symbol(member->Name)))
			{
				// fields
				auto desc = NanNew<Object>();
				desc->Set(NanNew<String>("name"), ToV8String(member->Name));
				desc->Set(NanNew<String>("type"), NanNew<String>("field"));
				auto access = NanNew<Array>();
				int index = 0;
				access->Set(NanNew<Number>(index++), NanNew<String>("get"));
				if (!fi->IsInitOnly)
				{
					access->Set(NanNew<Number>(index++), NanNew<String>("set"));
				}
				desc->Set(NanNew<String>("access"), access);
				obj->Set(ToV8Symbol(member->Name), desc);
			}

			auto mi = dynamic_cast<MethodInfo^>(member);
			if (mi != nullptr &&
				!mi->IsSpecialName &&
				!obj->Has(ToV8Symbol(member->Name)))
			{
				// methods
				auto desc = NanNew<Object>();
				desc->Set(NanNew<String>("name"), ToV8String(member->Name));
				desc->Set(NanNew<String>("type"), NanNew<String>("method"));
				obj->Set(ToV8Symbol(member->Name), desc);
			}

			auto pi = dynamic_cast<PropertyInfo^>(member);
			if (pi != nullptr &&
				!pi->IsSpecialName)
			{
				// properties
				auto desc = (obj->Has(ToV8Symbol(member->Name)))
					? Local<Object>::Cast(obj->Get(ToV8Symbol(member->Name)))
					: NanNew<Object>();
				desc->Set(NanNew<String>("name"), ToV8String(member->Name));
				desc->Set(NanNew<String>("type"), NanNew<String>("property"));
				
				auto access = (obj->Has(NanNew<String>("access")))
					? Local<Array>::Cast(obj->Get(NanNew<String>("access")))
					: NanNew<Array>();
				auto canGet = pi->CanRead;
				auto canSet = pi->CanWrite;
				for (int i = 0; i < (int)access->Length(); i++)
				{
					if (access->Get(NanNew<Number>(i))->StrictEquals(NanNew<String>("get")))
					{
						canGet = true;
					}
					if (access->Get(NanNew<Number>(i))->StrictEquals(NanNew<String>("set")))
					{
						canSet = true;
					}
				}
				int index = 0;
				if (canGet)
				{
					access->Set(NanNew<Number>(index++), NanNew<String>("get"));
				}
				if (canSet)
				{
					access->Set(NanNew<Number>(index++), NanNew<String>("set"));
				}
				desc->Set(NanNew<String>("access"), access);

				desc->Set(NanNew<String>("indexed"), NanNew<Boolean>(0 < pi->GetIndexParameters()->Length));
				
				obj->Set(ToV8Symbol(member->Name), desc);
			}

			auto ti = dynamic_cast<System::Type^>(member);
			if (ti != nullptr &&
				!ti->IsSpecialName &&
				!obj->Has(ToV8Symbol(member->Name)))
			{
				// nested typess
				auto desc = NanNew<Object>();
				desc->Set(NanNew<String>("name"), ToV8String(member->Name));
				desc->Set(NanNew<String>("type"), NanNew<String>("nestedType"));
				desc->Set(NanNew<String>("fullName"), ToV8String(ti->AssemblyQualifiedName));
				obj->Set(ToV8Symbol(member->Name), desc);
			}
		}

		NanReturnValue(obj);
	}

	
	// clr.invokeMethod(typeName, methodName, CLRObject, arguments) : returnValue
	//   invoke static or instance method
	//   - typeName: type name string, for static members
	//   - methodName: method name
	//   - CLRObject: CLR object instance, for instance members
	//   - arguments: array of method arguments
	//   - returnValue: return value of method, v8 primitive or CLR wrapped object
	static NAN_METHOD(InvokeMethod)
	{
		NanScope();

		if (args.Length() != 4 ||
			!args[0]->IsString() ||
			!args[1]->IsString() ||
			(!CLRObject::IsCLRObject(args[2]) && args[2]->BooleanValue() != false) ||
			!args[3]->IsArray())
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}

		Handle<Value> result;
		try
		{
			result = CLRBinder::InvokeMethod(
				args[0],
				args[1],
				args[2],
				args[3]);
		}
		catch (System::Exception^ ex)
		{
			NanThrowError(ToV8Error(ex));
			NanReturnUndefined();
		}

		NanReturnValue(result);
	}
	

	// clr.getField(typeName, fieldName, CLRObject) : returnValue
	//   invoke field getter
	//   - typeName: type name string, for static members
	//   - fieldName: field name
	//   - CLRObject: CLR object instance, for instance members
	//   - returnValue: field value, v8 primitive or CLR wrapped object
	static NAN_METHOD(GetField)
	{
		NanScope();

		if (args.Length() != 3 ||
			!args[0]->IsString() ||
			!args[1]->IsString() ||
			(!CLRObject::IsCLRObject(args[2]) && args[2]->BooleanValue() != false))
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}

		Handle<Value> result;
		try
		{
			result = CLRBinder::GetField(
				args[0],
				args[1],
				args[2]);
		}
		catch (System::Exception^ ex)
		{
			NanThrowError(ToV8Error(ex));
			NanReturnUndefined();
		}

		NanReturnValue(result);
	}
	

	// clr.setField(typeName | CLRObject, fieldName, value)
	//   invoke field setter
	//   - typeName: type name string, for static members
	//   - CLRObject: CLR object instance, for instance members
	//   - fieldName: field name
	//   - value: field value, v8 primitive or CLR wrapped object
	static NAN_METHOD(SetField)
	{
		NanScope();

		if (args.Length() != 4 ||
			!args[0]->IsString() ||
			!args[1]->IsString() ||
			(!CLRObject::IsCLRObject(args[2]) && args[2]->BooleanValue() != false) ||
			!args[3].IsEmpty())
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}

		try
		{
			CLRBinder::SetField(
				args[0],
				args[1],
				args[2],
				args[3]);
		}
		catch (System::Exception^ ex)
		{
			NanThrowError(ToV8Error(ex));
			NanReturnUndefined();
		}

		NanReturnUndefined();
	}
	

	// clr.isCLRObject(obj) : boolean
	//   returns if specified object is CLR wrapped object
	//   - obj: CLR wrapped object or any javascript value
	static NAN_METHOD(IsCLRObject)
	{
		NanScope();

		if (args.Length() != 1 ||
			args[0].IsEmpty())
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}

		NanReturnValue(NanNew<Boolean>(CLRObject::IsCLRObject(args[0])));
	}

	static NAN_METHOD(GetType)
	{
		NanScope();

		if (args.Length() != 1 ||
			!CLRObject::IsCLRObject(args[0]))
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}

		NanReturnValue(CLRObject::GetType(args[0]));
	}

	static NAN_METHOD(IsCLRConstructor)
	{
		NanScope();

		if (args.Length() != 1 ||
			args[0].IsEmpty())
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}
		
		NanReturnValue(CLRObject::TypeOf(args[0]));
	}
	
	static NAN_METHOD(TypeOf)
	{
		NanScope();

		if (args.Length() != 1 ||
			!CLRObject::IsCLRConstructor(args[0]))
		{
			NanThrowTypeError("Arguments does not match it's parameter list");
			NanReturnUndefined();
		}

		NanReturnValue(CLRObject::TypeOf(args[0]));
	}

	// resolve assemblies which is loaded by reflection
	static Assembly^ ResolveAssembly(System::Object^ sender, System::ResolveEventArgs^ ea)
	{
		for each (auto assembly in System::AppDomain::CurrentDomain->GetAssemblies())
		{
			if (assembly->FullName == ea->Name)
			{
				return assembly;
			}
		}

		return nullptr;
	}

public:
	static void Init(Handle<Object> exports)
	{
		CLRObject::Init();

		exports->Set(NanNew<String>("import"), NanNew<FunctionTemplate>(Import)->GetFunction());
		exports->Set(NanNew<String>("getAssemblies"), NanNew<FunctionTemplate>(GetAssemblies)->GetFunction());
		exports->Set(NanNew<String>("getTypes"), NanNew<FunctionTemplate>(GetTypes)->GetFunction());
		exports->Set(NanNew<String>("createConstructor"), NanNew<FunctionTemplate>(CreateConstructor)->GetFunction());
		exports->Set(NanNew<String>("getMembers"), NanNew<FunctionTemplate>(GetMembers)->GetFunction());
		exports->Set(NanNew<String>("invokeMethod"), NanNew<FunctionTemplate>(InvokeMethod)->GetFunction());
		exports->Set(NanNew<String>("getField"), NanNew<FunctionTemplate>(GetField)->GetFunction());
		exports->Set(NanNew<String>("setField"), NanNew<FunctionTemplate>(SetField)->GetFunction());
		exports->Set(NanNew<String>("isCLRObject"), NanNew<FunctionTemplate>(IsCLRObject)->GetFunction());
		exports->Set(NanNew<String>("getType"), NanNew<FunctionTemplate>(GetType)->GetFunction());
		exports->Set(NanNew<String>("isCLRConstructor"), NanNew<FunctionTemplate>(IsCLRConstructor)->GetFunction());
		exports->Set(NanNew<String>("typeOf"), NanNew<FunctionTemplate>(TypeOf)->GetFunction());

		System::AppDomain::CurrentDomain->AssemblyResolve += gcnew System::ResolveEventHandler(
			&CLR::ResolveAssembly);
	}
};

#pragma managed(push, off)
NODE_MODULE(clr, CLR::Init);
#pragma managed(pop)

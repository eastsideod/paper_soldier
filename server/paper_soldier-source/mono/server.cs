using funapi;
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

// These flags are defined in the paper_soldier_server.cc file.
// You can refer to them in C# code as Flags.GetString("example_arg3").
//
// DEFINE_string(example_arg3, "default_val", "example flag")
// DEFINE_int32(example_arg4, 100, "example flag");
// DEFINE_bool(example_arg5, false, "example flag");


namespace PaperSoldier
{
	public class Server
	{
		public static bool Install(ArgumentMap arguments)
		{
			// Session open, close handlers.
			NetworkHandlerRegistry.RegisterSessionHandler (
				new NetworkHandlerRegistry.SessionOpenedHandler (OnSessionOpened),
				new NetworkHandlerRegistry.SessionClosedHandler (OnSessionClosed));

			// "echo" message handler for JSON type.
			NetworkHandlerRegistry.RegisterMessageHandler ("echo", new NetworkHandlerRegistry.JsonMessageHandler (OnEcho));

			// "echo_pbuf" message handler for Google Protocol Buffers.
			NetworkHandlerRegistry.RegisterMessageHandler ("pbuf_echo", new NetworkHandlerRegistry.ProtobufMessageHandler (OnPbufEcho));

			// Parameters specified in the "arguments" section in your MANIFEST.json
			// will be passed in the variable "arguments".
			// So, you can give configuration data to your game server.
			//
			// Example:
			//
			// We have in MANIFEST.json "example_arg1" and "example_arg2" that
			// have a string value and an integer value, respectively.
			// So, you can access the arguments like below:
			string arg1 = arguments.FindString ("example_arg1");
			Log.Info ("example_arg1: {0}", arg1);

			Int64 arg2 = arguments.FindInteger ("example_arg2");
			Log.Info ("example_arg2: {0}", arg2);

			// You can override gflag like this: ./paper_soldier-local --example_arg3=hahaha
			Log.Info("example_arg3: {0}", Flags.GetString ("example_arg3"));

			// Registers a timer.
			//
			// Below demonstrates a repeating timer. One-shot timer is also available.
			// Please see the Timer class.
			Timer.ExpireRepeatedly(WallClock.FromSec(1), OnTick);

			return true;
		}

		public static bool Start()
		{
			Log.Info ("Hello, {0}!", "PaperSoldier");
			return true;
		}

		public static bool Uninstall()
		{
			return true;
		}

		public static void OnSessionOpened(Session session)
		{
			Log.Info ("Session opened: session_id={0}", session.Id);
		}

		public static void OnSessionClosed(Session session, Session.CloseReason reason)
		{
			Log.Info ("Session closed: session_id={0}, reason={1}", session.Id, reason);
		}

		public static void OnEcho (Session session, JObject message)
		{
			Log.Info (message.ToString ());
			session.SendMessage ("echo", message);
		}

		public static void OnPbufEcho(Session session, FunMessage message)
		{
			PbufEchoMessage echo;
			if (!message.TryGetExtension_pbuf_echo(out echo)) {
				Log.Error ("OnPbufEcho: Wrong message.");
				return;
			}
			Log.Info (echo.msg);

			FunMessage reply = new FunMessage ();
			reply.AppendExtension_pbuf_echo(echo);
			session.SendMessage ("pbuf_echo", reply);
		}

		// Timer handler.
		//
		// (Just for your reference. Please replace with your own.)
		public static void OnTick(UInt64 timer_id, DateTime clock)
		{
		}
	}
}

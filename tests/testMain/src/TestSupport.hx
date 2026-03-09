import haxe.io.Bytes;
import haxe.io.Path;
import miniaudio.Miniaudio;
import sys.FileSystem;

typedef FixtureDecode =
{
	path:String,
	bytes:Bytes,
	floatDecoded:miniaudio.DecodedAudio,
	pcm16Decoded:miniaudio.DecodedAudio,
}

typedef FixtureFailure =
{
	path:String,
	message:String,
}

typedef FixtureIssue =
{
	path:String,
	message:String,
	kind:String,
}

class TestSupport
{
	public static final fixtures = loadFixtures();
	public static final supportedExtensions = ['wav', 'mp3', 'flac', 'ogg', 'opus'];

	static function loadFixtures():Array<String>
	{
		final baseDir = 'audio/formats';
		final result = new Array<String>();

		for (name in FileSystem.readDirectory(baseDir))
		{
			final path = baseDir + '/' + name;
			if (FileSystem.isDirectory(path))
				continue;

			if (Path.extension(name) != '')
				result.push(path);
		}

		result.sort(Reflect.compare);
		assert(result.length > 0, 'No audio fixtures found in ' + baseDir);
		return result;
	}

	public static function assert(condition:Bool, message:String):Void
	{
		if (!condition)
			throw message;
	}

	public static function assertEquals<T>(expected:T, actual:T, message:String):Void
	{
		if (expected != actual)
			throw message + ' (expected=' + Std.string(expected) + ', actual=' + Std.string(actual) + ')';
	}

	public static function assertNear(expected:Float, actual:Float, epsilon:Float, message:String):Void
	{
		if (Math.abs(expected - actual) > epsilon)
			throw message + ' (expected=' + expected + ', actual=' + actual + ')';
	}

	public static function assertBytesEqual(expected:Bytes, actual:Bytes, message:String):Void
	{
		assertEquals(expected.length, actual.length, message + ': byte length mismatch');
		for (i in 0...expected.length)
			if (expected.get(i) != actual.get(i))
				throw message + ' at byte ' + i;
	}

	public static function fixtureLabel(path:String):String
	{
		return Path.withoutDirectory(path);
	}

	public static function printPlay(label:String):Void
	{
		Sys.println('PLAY ' + label);
	}

	public static function printOk(?label:String):Void
	{
		Sys.println('OK' + (label != null ? '   ' + label : ''));
	}

	public static function printFail(label:String, message:String):Void
	{
		Sys.println('FAIL ' + label + ': ' + message);
	}

	public static function printUnsupported(label:String, message:String):Void
	{
		final prefix = label + ': ';
		final cleanMessage = StringTools.startsWith(message, prefix) ? message.substr(prefix.length) : message;
		Sys.println('UNSUPPORTED ' + label + ' (' + cleanMessage + ')');
	}

	public static function classifyIssue(path:String, message:String):String
	{
		final ext = Path.extension(path).toLowerCase();
		if (supportedExtensions.indexOf(ext) == -1)
			return 'unsupported';

		return 'fail';
	}

	public static function decodeFixture(path:String):FixtureDecode
	{
		final bytes = sys.io.File.getBytes(path);
		final floatDecoded = Miniaudio.decodeToPCMFloat(bytes);
		assert(floatDecoded != null, fixtureLabel(path) + ': float decode failed: ' + Miniaudio.describeLastError());
		assertDecodedAudio(floatDecoded, fixtureLabel(path), true);

		final pcm16Decoded = Miniaudio.decodeToPCM16(bytes);
		assert(pcm16Decoded != null, fixtureLabel(path) + ': s16 decode failed: ' + Miniaudio.describeLastError());
		assertDecodedAudio(pcm16Decoded, fixtureLabel(path), false);

		assertEquals(floatDecoded.channels, pcm16Decoded.channels, fixtureLabel(path) + ': channel mismatch between decode paths');
		assertEquals(floatDecoded.sampleRate, pcm16Decoded.sampleRate, fixtureLabel(path) + ': sample rate mismatch between decode paths');
		assertEquals(floatDecoded.samples, pcm16Decoded.samples, fixtureLabel(path) + ': sample count mismatch between decode paths');

		return {
			path: path,
			bytes: bytes,
			floatDecoded: floatDecoded,
			pcm16Decoded: pcm16Decoded,
		};
	}

	public static function assertDecodedAudio(decoded:miniaudio.DecodedAudio, label:String, floatFormat:Bool):Void
	{
		assert(decoded.channels > 0, label + ': invalid channel count');
		assert(decoded.sampleRate > 0, label + ': invalid sample rate');
		assert(decoded.samples > 0, label + ': invalid sample count');
		assertEquals(floatFormat, decoded.floatFormat, label + ': unexpected format flag');
		final bytesPerSample = floatFormat ? 4 : 2;
		assertEquals(decoded.samples * decoded.channels * bytesPerSample, decoded.bytes.length, label + ': unexpected decoded byte count');
	}
}

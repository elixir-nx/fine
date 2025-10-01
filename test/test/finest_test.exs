defmodule FinestTest do
  use ExUnit.Case, async: true

  alias Finest.NIF

  test "add" do
    assert NIF.add(1, 2) == 3
  end

  describe "codec" do
    test "term" do
      assert NIF.codec_term(10) == 10
      assert NIF.codec_term("hello world") == "hello world"
      assert NIF.codec_term([1, 2, 3]) == [1, 2, 3]
    end

    test "int64" do
      assert NIF.codec_int64(10) == 10
      assert NIF.codec_int64(-10) == -10

      assert_raise ArgumentError, "decode failed, expected an integer", fn ->
        NIF.codec_int64(10.0)
      end
    end

    test "uint64" do
      assert NIF.codec_uint64(10)

      assert_raise ArgumentError, "decode failed, expected an unsigned integer", fn ->
        NIF.codec_uint64(-10)
      end
    end

    test "double" do
      assert NIF.codec_double(10.0) == 10.0
      assert NIF.codec_double(-10.0) == -10.0

      assert_raise ArgumentError, "decode failed, expected a float", fn ->
        NIF.codec_double(1)
      end
    end

    test "bool" do
      assert NIF.codec_bool(true) == true
      assert NIF.codec_bool(false) == false

      assert_raise ArgumentError, "decode failed, expected a boolean", fn ->
        NIF.codec_bool(1)
      end
    end

    test "pid" do
      assert NIF.codec_pid(self()) == self()

      assert_raise ArgumentError, "decode failed, expected a local pid", fn ->
        NIF.codec_pid(1)
      end
    end

    test "binary" do
      assert NIF.codec_binary("hello world") == "hello world"
      assert NIF.codec_binary(<<0, 1, 2>>) == <<0, 1, 2>>
      assert NIF.codec_binary(<<>>) == <<>>

      assert_raise ArgumentError, "decode failed, expected a binary", fn ->
        NIF.codec_binary(1)
      end
    end

    test "string_view" do
      assert NIF.codec_string_view("hello world") == "hello world"
      assert NIF.codec_string_view(<<0, 1, 2>>) == <<0, 1, 2>>
      assert NIF.codec_string_view(<<>>) == <<>>

      assert_raise ArgumentError, "decode failed, expected a binary", fn ->
        NIF.codec_string(1)
      end
    end

    test "string" do
      assert NIF.codec_string("hello world") == "hello world"
      assert NIF.codec_string(<<0, 1, 2>>) == <<0, 1, 2>>
      assert NIF.codec_string(<<>>) == <<>>

      assert NIF.codec_string_alloc("hello world") == "hello world"
      assert NIF.codec_string_alloc(<<0, 1, 2>>) == <<0, 1, 2>>
      assert NIF.codec_string_alloc(<<>>) == <<>>

      assert_raise ArgumentError, "decode failed, expected a binary", fn ->
        NIF.codec_string(1)
      end
    end

    test "atom" do
      assert NIF.codec_atom(:hello) == :hello

      # NIF APIs support UTF8 atoms since OTP 26
      if System.otp_release() >= "26" do
        assert NIF.codec_atom(:"🦊 in a 📦") == :"🦊 in a 📦"
      end

      assert_raise ArgumentError, "decode failed, expected an atom", fn ->
        NIF.codec_atom(1)
      end
    end

    test "nullopt" do
      assert NIF.codec_nullopt() == nil
    end

    test "optional" do
      assert NIF.codec_optional_int64(10) == 10
      assert NIF.codec_optional_int64(nil) == nil

      assert_raise ArgumentError, "decode failed, expected an integer", fn ->
        NIF.codec_optional_int64(10.0)
      end
    end

    test "variant" do
      assert NIF.codec_variant_int64_or_string(10) == 10
      assert NIF.codec_variant_int64_or_string("hello world") == "hello world"

      assert_raise ArgumentError,
                   "decode failed, none of the variant types could be decoded",
                   fn ->
                     NIF.codec_variant_int64_or_string(10.0)
                   end
    end

    test "tuple" do
      assert NIF.codec_tuple_int64_and_string({10, "hello world"}) == {10, "hello world"}

      assert_raise ArgumentError, "decode failed, expected a tuple", fn ->
        NIF.codec_tuple_int64_and_string(10)
      end

      assert_raise ArgumentError,
                   "decode failed, expected tuple to have 2 elements, but had 0",
                   fn ->
                     NIF.codec_tuple_int64_and_string({})
                   end

      assert_raise ArgumentError, "decode failed, expected a binary", fn ->
        NIF.codec_tuple_int64_and_string({10, 10})
      end
    end

    test "vector" do
      assert NIF.codec_vector_int64([1, 2, 3]) == [1, 2, 3]
      assert NIF.codec_vector_int64_alloc([1, 2, 3]) == [1, 2, 3]

      assert_raise ArgumentError, "decode failed, expected a list", fn ->
        NIF.codec_vector_int64(10)
      end

      assert_raise ArgumentError, "decode failed, expected an integer", fn ->
        NIF.codec_vector_int64([10.0])
      end
    end

    test "map" do
      small_map = %{hello: 1, world: 2}

      empty_map = %{}

      # Large maps have more than 32 elements:
      #     https://www.erlang.org/doc/system/maps.html#how-large-maps-are-implemented
      large_map =
        0..64
        |> Enum.with_index()
        |> Map.new(fn {key, value} -> {:"a#{key}", value} end)

      for map <- [small_map, empty_map, large_map] do
        assert NIF.codec_map_atom_int64(map) == map
        assert NIF.codec_map_atom_int64_alloc(map) == map
        assert NIF.codec_unordered_map_atom_int64(map) == map
        assert NIF.codec_unordered_map_atom_int64_alloc(map) == map
      end

      invalid_map = 10

      assert_raise ArgumentError, "decode failed, expected a map", fn ->
        NIF.codec_map_atom_int64(invalid_map)
      end

      assert_raise ArgumentError, "decode failed, expected a map", fn ->
        NIF.codec_map_atom_int64_alloc(invalid_map)
      end

      assert_raise ArgumentError, "decode failed, expected a map", fn ->
        NIF.codec_unordered_map_atom_int64(invalid_map)
      end

      assert_raise ArgumentError, "decode failed, expected a map", fn ->
        NIF.codec_unordered_map_atom_int64_alloc(invalid_map)
      end

      map_with_invalid_key = %{"hello" => 1}

      assert_raise ArgumentError, "decode failed, expected an atom", fn ->
        NIF.codec_map_atom_int64(map_with_invalid_key)
      end

      assert_raise ArgumentError, "decode failed, expected an atom", fn ->
        NIF.codec_map_atom_int64_alloc(map_with_invalid_key)
      end

      assert_raise ArgumentError, "decode failed, expected an atom", fn ->
        NIF.codec_unordered_map_atom_int64(map_with_invalid_key)
      end

      assert_raise ArgumentError, "decode failed, expected an atom", fn ->
        NIF.codec_unordered_map_atom_int64_alloc(map_with_invalid_key)
      end

      map_with_invalid_value = %{hello: :world}

      assert_raise ArgumentError, "decode failed, expected an integer", fn ->
        NIF.codec_map_atom_int64(map_with_invalid_value)
      end

      assert_raise ArgumentError, "decode failed, expected an integer", fn ->
        NIF.codec_map_atom_int64_alloc(map_with_invalid_value)
      end

      assert_raise ArgumentError, "decode failed, expected an integer", fn ->
        NIF.codec_unordered_map_atom_int64(map_with_invalid_value)
      end

      assert_raise ArgumentError, "decode failed, expected an integer", fn ->
        NIF.codec_unordered_map_atom_int64_alloc(map_with_invalid_value)
      end
    end

    test "keyword" do
      empty_keyword = []

      small_keyword = [hello: 1, world: 2]

      large_keyword =
        0..64 |> Enum.map(fn x -> {:"a#{x}", x} end) |> Enum.to_list()

      for keyword <- [empty_keyword, small_keyword, large_keyword] do
        assert Enum.sort(NIF.codec_multimap_atom_int64(keyword)) == Enum.sort(keyword)
        assert Enum.sort(NIF.codec_multimap_atom_int64_alloc(keyword)) == Enum.sort(keyword)
        assert Enum.sort(NIF.codec_unordered_multimap_atom_int64(keyword)) == Enum.sort(keyword)

        assert Enum.sort(NIF.codec_unordered_multimap_atom_int64_alloc(keyword)) ==
                 Enum.sort(keyword)
      end

      invalid_keyword = 10

      assert_raise ArgumentError, "decode failed, expected a list", fn ->
        NIF.codec_multimap_atom_int64(invalid_keyword)
      end

      assert_raise ArgumentError, "decode failed, expected a list", fn ->
        NIF.codec_multimap_atom_int64_alloc(invalid_keyword)
      end

      assert_raise ArgumentError, "decode failed, expected a list", fn ->
        NIF.codec_unordered_multimap_atom_int64(invalid_keyword)
      end

      assert_raise ArgumentError, "decode failed, expected a list", fn ->
        NIF.codec_unordered_multimap_atom_int64_alloc(invalid_keyword)
      end

      keyword_with_invalid_key = [{"hello", 42}]

      assert_raise ArgumentError, "decode failed, expected an atom", fn ->
        NIF.codec_multimap_atom_int64(keyword_with_invalid_key)
      end

      assert_raise ArgumentError, "decode failed, expected an atom", fn ->
        NIF.codec_multimap_atom_int64_alloc(keyword_with_invalid_key)
      end

      assert_raise ArgumentError, "decode failed, expected an atom", fn ->
        NIF.codec_unordered_multimap_atom_int64(keyword_with_invalid_key)
      end

      assert_raise ArgumentError, "decode failed, expected an atom", fn ->
        NIF.codec_unordered_multimap_atom_int64_alloc(keyword_with_invalid_key)
      end

      keyword_with_invalid_value = [hello: 1.0]

      assert_raise ArgumentError, "decode failed, expected an integer", fn ->
        NIF.codec_multimap_atom_int64(keyword_with_invalid_value)
      end

      assert_raise ArgumentError, "decode failed, expected an integer", fn ->
        NIF.codec_multimap_atom_int64_alloc(keyword_with_invalid_value)
      end

      assert_raise ArgumentError, "decode failed, expected an integer", fn ->
        NIF.codec_unordered_multimap_atom_int64(keyword_with_invalid_value)
      end

      assert_raise ArgumentError, "decode failed, expected an integer", fn ->
        NIF.codec_unordered_multimap_atom_int64_alloc(keyword_with_invalid_value)
      end
    end

    test "resource" do
      resource = NIF.resource_create(self())
      assert is_reference(resource)

      assert NIF.codec_resource(resource) == resource

      assert_raise ArgumentError, "decode failed, expected a resource reference", fn ->
        NIF.codec_resource(10)
      end
    end

    test "struct" do
      struct = %Finest.Point{x: 1, y: 2}
      assert NIF.codec_struct(struct) == struct

      assert_raise ArgumentError, "decode failed, expected a struct", fn ->
        NIF.codec_struct(10)
      end

      assert_raise ArgumentError, "decode failed, expected a struct", fn ->
        NIF.codec_struct(%{})
      end

      assert_raise ArgumentError, "decode failed, expected a Elixir.Finest.Point struct", fn ->
        NIF.codec_struct(~D"2000-01-01")
      end
    end

    test "exception struct" do
      struct = %Finest.Error{data: 1}
      assert NIF.codec_struct_exception(struct) == struct
      assert is_exception(NIF.codec_struct_exception(struct))

      assert_raise ArgumentError, "decode failed, expected a struct", fn ->
        NIF.codec_struct_exception(10)
      end
    end

    test "ok tagged tuple" do
      assert NIF.codec_ok_empty() == :ok
      assert NIF.codec_ok_int64(10) == {:ok, 10}
    end

    test "error tagged tuple" do
      assert NIF.codec_error_empty() == :error
      assert NIF.codec_error_string("this is the reason") == {:error, "this is the reason"}
    end

    test "result" do
      assert NIF.codec_result_string_int64_ok("fine") == {:ok, "fine"}
      assert NIF.codec_result_string_int64_error(42) == {:error, 42}
      assert NIF.codec_result_string_int64_ok_conversion() == {:ok, "fine"}
      assert NIF.codec_result_string_int64_error_conversion() == {:error, 42}
      assert NIF.codec_result_int64_string_void_ok_conversion() == {:ok, 201_702, "c++17"}
    end
  end

  describe "resource" do
    test "survives across NIF calls" do
      resource = NIF.resource_create(self())
      assert NIF.resource_get(resource) == self()
    end

    test "calls destructors when garbage collected" do
      NIF.resource_create(self())
      :erlang.garbage_collect(self())

      assert_receive :destructor_with_env
      assert_receive :destructor_default
    end

    test "resource binary keeps reference to the resource" do
      _ =
        (fn ->
           binary = NIF.resource_binary(NIF.resource_create(self()))
           :erlang.garbage_collect(self())

           # We have reference to the binary, so the resource should
           # stay alive.
           refute_receive :destructor_default, 10

           byte_size(binary)
         end).()

      # We no longer have reference to the binary, so GC should destroy
      # the resource.
      :erlang.garbage_collect(self())

      assert_receive :destructor_default
    end
  end

  describe "make_new_binary" do
    test "creates a binary term copying the original buffer" do
      assert NIF.make_new_binary() == "hello world"
    end
  end

  describe "exceptions" do
    test "standard exceptions" do
      assert_raise RuntimeError, "runtime error reason", fn ->
        NIF.throw_runtime_error()
      end

      assert_raise ArgumentError, "invalid argument reason", fn ->
        NIF.throw_invalid_argument()
      end

      assert_raise RuntimeError, "unknown exception thrown within NIF", fn ->
        NIF.throw_other_exception()
      end
    end

    test "raising an elixir exception" do
      assert_raise Finest.Error, "got error with data 10", fn ->
        NIF.raise_elixir_exception()
      end
    end

    test "raising any term" do
      assert_raise ErlangError, "Erlang error: :oops", fn ->
        NIF.raise_erlang_error()
      end
    end
  end

  describe "mutex" do
    test "unique_lock" do
      NIF.mutex_unique_lock_test()
    end

    test "scoped_lock" do
      NIF.mutex_scoped_lock_test()
    end
  end

  describe "shared_mutex" do
    test "unique_lock" do
      NIF.shared_mutex_unique_lock_test()
    end

    test "shared_lock" do
      NIF.shared_mutex_shared_lock_test()
    end
  end

  describe "condition_variable" do
    test "condition_variable" do
      NIF.condition_variable_test()
    end
  end

  describe "comparison" do
    test "equal" do
      refute NIF.compare_eq(64, 42)
      refute NIF.compare_eq(nil, %{})
      assert NIF.compare_eq("fine", "fine")
    end

    test "not equal" do
      assert NIF.compare_ne(64, 42)
      assert NIF.compare_ne(nil, %{})
      refute NIF.compare_ne("fine", "fine")
    end

    test "less than" do
      refute NIF.compare_lt(64, 42)
      assert NIF.compare_lt(nil, %{})
      refute NIF.compare_lt("fine", "fine")
    end

    test "less than equal" do
      refute NIF.compare_le(64, 42)
      assert NIF.compare_le(nil, %{})
      assert NIF.compare_le("fine", "fine")
    end

    test "greater than" do
      assert NIF.compare_gt(64, 42)
      refute NIF.compare_gt(nil, %{})
      refute NIF.compare_gt("fine", "fine")
    end

    test "greater than equal" do
      assert NIF.compare_ge(64, 42)
      refute NIF.compare_ge(nil, %{})
      assert NIF.compare_ge("fine", "fine")
    end
  end

  describe "hash" do
    test "term" do
      for value <- [42, "fine", ["it", %{"should" => {"just", "work"}}], :atom] do
        assert NIF.hash_term(value) == NIF.hash_term(value)
      end
    end

    test "atom" do
      for value <- [:ok, :error, :"with spaces", Enum, nil, true, false] do
        assert NIF.hash_atom(value) == NIF.hash_atom(value)
      end
    end
  end
end

#! /usr/bin/gsr

function function_one()
{
  function_two();
}

function function_two()
{
  function_three();
}

function function_three()
{
  throw new Error("An error occurred");
}

function_one();
